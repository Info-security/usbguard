//
// Copyright (C) 2015 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#include "LinuxDeviceManager.hpp"
#include "LinuxSysIO.hpp"
#include "LoggerPrivate.hpp"
#include <USB.hpp>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <stdexcept>
#include <fstream>
#include <unistd.h>

namespace usbguard {

  LinuxDevice::LinuxDevice(LinuxDeviceManager& device_manager, struct udev_device* dev)
    : Device(device_manager)
  {
    logger->debug("Creating a new LinuxDevice instance");

    /*
     * Look for the parent USB device and set the parent id
     * if we find one.
     */
    struct udev_device *parent_dev = udev_device_get_parent(dev);

    if (parent_dev == nullptr) {
      throw std::runtime_error("Cannot identify the parent device");
    }

    const char *parent_devtype = udev_device_get_devtype(parent_dev);
    const char *parent_syspath_cstr = udev_device_get_syspath(parent_dev);

    if (parent_syspath_cstr == nullptr) {
      throw std::runtime_error("Cannot retrieve syspath of the parent device");
    }

    const String parent_syspath(parent_syspath_cstr);

    logger->debug("Parent device syspath: {}", parent_syspath_cstr);

    if (parent_devtype == nullptr ||
        strcmp(parent_devtype, "usb_device") != 0) {
      /* The parent device is not a USB device */
      setParentID(Rule::RootID);
      setParentHash(hashString(parent_syspath));
    }
    else {
      setParentID(device_manager.getIDFromSysPath(parent_syspath));
    }

    const char *name = udev_device_get_sysattr_value(dev, "product");
    if (name) {
      logger->debug("DeviceName={}", name);
      setName(name);
    }
    
    const char *id_vendor_cstr = udev_device_get_sysattr_value(dev, "idVendor");
    const char *id_product_cstr = udev_device_get_sysattr_value(dev, "idProduct");

    if (id_vendor_cstr && id_product_cstr) {
      logger->debug("VendorID={}", id_vendor_cstr);
      logger->debug("ProductID={}", id_product_cstr);
      const String id_vendor = id_vendor_cstr;
      const String id_product = id_product_cstr;
      USBDeviceID device_id(id_vendor, id_product);
      setDeviceID(device_id);
    }

    const char *serial = udev_device_get_sysattr_value(dev, "serial");
    if (serial) {
      logger->debug("Serial={}", serial);
      setSerial(serial);
    }

    /* FIXME: We should somehow lock the syspath before accessing the
     *        files inside to prevent creating invalid devices. It is
     *        possible that the device we are working with now will not
     *        be the same when we start reading the descriptor data and
     *        the authorization state.
     */
    const char *syspath = udev_device_get_syspath(dev);
    if (syspath) {
      logger->debug("Syspath={}", syspath);
      _syspath = syspath;
    } else {
      throw std::runtime_error("device wihtout syspath");
    }

    const char *sysname = udev_device_get_sysname(dev);
    if (sysname) {
      logger->debug("Sysname={}", sysname);
      setPort(sysname);
    } else {
      throw std::runtime_error("device wihtout sysname");
    }

    setTarget(Rule::Target::Unknown);
    std::ifstream authstate_stream(_syspath + "/authorized", std::ifstream::binary);

    if (!authstate_stream.good()) {
      throw std::runtime_error("cannot read authorization state");
    }
    else {
      switch(authstate_stream.get()) {
        case '1':
          setTarget(Rule::Target::Allow);
          break;
        case '0':
          setTarget(Rule::Target::Block);
          break;
        default:
          /* Block the device if we get an unexpected value */
          setTarget(Rule::Target::Block);
      }
      logger->debug("Authstate={}", Rule::targetToString(getTarget()));
    }

    std::ifstream descriptor_stream(_syspath + "/descriptors", std::ifstream::binary);

    /* Find out the descriptor data stream size */
    size_t descriptor_expected_size = 0;

    if (!descriptor_stream.good()) {
      throw std::runtime_error("Cannot load USB descriptors: failed to open the descriptor data stream");
    }
    else {
      using namespace std::placeholders;
      USBDescriptorParser parser;

      auto load_device_descriptor = std::bind(&LinuxDevice::loadDeviceDescriptor, this, _1, _2);
      auto load_configuration_descriptor = std::bind(&LinuxDevice::loadConfigurationDescriptor, this, _1, _2);
      auto load_interface_descriptor = std::bind(&LinuxDevice::loadInterfaceDescriptor, this, _1, _2);
      auto load_endpoint_descriptor = std::bind(&LinuxDevice::loadEndpointDescriptor, this, _1, _2);

      parser.setHandler(USB_DESCRIPTOR_TYPE_DEVICE, sizeof (USBDeviceDescriptor),
                        USBParseDeviceDescriptor, load_device_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_CONFIGURATION, sizeof (USBConfigurationDescriptor),
                        USBParseConfigurationDescriptor, load_configuration_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_INTERFACE, sizeof (USBInterfaceDescriptor),
                        USBParseInterfaceDescriptor, load_interface_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_ENDPOINT, sizeof (USBEndpointDescriptor),
                        USBParseEndpointDescriptor, load_endpoint_descriptor);
      parser.setHandler(USB_DESCRIPTOR_TYPE_ENDPOINT, sizeof (USBAudioEndpointDescriptor),
                        USBParseAudioEndpointDescriptor, load_endpoint_descriptor);

      if ((descriptor_expected_size = parser.parse(descriptor_stream)) < sizeof(USBDeviceDescriptor)) {
        throw std::runtime_error("Descriptor data parsing failed: parser processed less data than the size of a USB device descriptor");
      }
    }

    logger->debug("Expected descriptor data size is {} byte(s)", descriptor_expected_size);

    /*
     * Reset descriptor stream before before computing
     * the device hash.
     *
     * Because the eofbit is set, clear() has to be called
     * before seekg().
     */
    descriptor_stream.clear();
    descriptor_stream.seekg(0);

    /*
     * Compute and set the device hash.
     */
    updateHash(descriptor_stream, descriptor_expected_size);

    logger->debug("DeviceHash={}", getHash());
    return;
  }

  const String& LinuxDevice::getSysPath() const
  {
    return _syspath;
  }

  bool LinuxDevice::isController() const
  {
    if (getPort().substr(0, 3) != "usb" || getInterfaceTypes().size() != 1) {
      return false;
    }

    const USBInterfaceType hub_interface("09:00:*");

    return hub_interface.appliesTo(getInterfaceTypes()[0]);
  }

  /*
   * Manager
   */
  LinuxDeviceManager::LinuxDeviceManager(DeviceManagerHooks& hooks)
    : DeviceManager(hooks),
      _thread(this, &LinuxDeviceManager::thread)
  {
    setDefaultBlockedState(/*state=*/true);

    if ((_event_fd = eventfd(0, 0)) < 0) {
      throw std::runtime_error("eventfd init error");
    }
    
    if ((_udev = udev_new()) == nullptr) {
      throw std::runtime_error("udev init error");
    }

    if ((_umon = udev_monitor_new_from_netlink(_udev, "udev")) == nullptr) {
      udev_unref(_udev);
      throw std::runtime_error("udev_monitor init error");
    }

    udev_monitor_filter_add_match_subsystem_devtype(_umon, "usb", "usb_device");
    return;
  }

  LinuxDeviceManager::~LinuxDeviceManager()
  {
    setDefaultBlockedState(/*state=*/false); // FIXME: Set to previous state
    stop();
    udev_monitor_unref(_umon);
    udev_unref(_udev);
    close(_event_fd);
    return;
  }

  void LinuxDeviceManager::setDefaultBlockedState(bool state)
  {
    sysioSetAuthorizedDefault(!state);
    return;
  }

  void LinuxDeviceManager::start()
  {
    // enumerate devices
    // broadcast present devices
    // start monitor thread
    _thread.start();
    return;
  }

  void LinuxDeviceManager::stop()
  {
    // stop monitor
    _thread.stop(/*do_wait=*/false);
    { /* Wakeup the device manager thread */
      const uint64_t one = 1;
      write(_event_fd, &one, sizeof one);
    }
    _thread.wait();
    return;
  }

  void LinuxDeviceManager::scan()
  {
    if (!_thread.running()) {
      udevEnumerateDevices();
    } else {
      throw std::runtime_error("DeviceManager thread is running, cannot perform a scan");
    }
    return;
  }

  Pointer<Device> LinuxDeviceManager::allowDevice(uint32_t id)
  {
    //log->debug("Allowing device {}", id);
    Pointer<Device> device = applyDevicePolicy(id, Rule::Target::Allow);
    //log->debug("Calling DeviceAllowed hook");
    DeviceAllowed(device);
    return device;
  }

  Pointer<Device> LinuxDeviceManager::blockDevice(uint32_t id)
  {
    //log->debug("Blocking device {}", id);
    Pointer<Device> device = applyDevicePolicy(id, Rule::Target::Block);
    //log->debug("Calling DeviceBlocked hook");
    DeviceBlocked(device);
    return device;
  }

  Pointer<Device> LinuxDeviceManager::rejectDevice(uint32_t id)
  {
    //log->debug("Rejecting device {}", id);
    Pointer<Device> device = applyDevicePolicy(id, Rule::Target::Reject);
    //log->debug("Calling DeviceRejected hook");
    DeviceRejected(device);
    return device;
  }

  Pointer<Device> LinuxDeviceManager::applyDevicePolicy(uint32_t id, Rule::Target target)
  {
    //log->debug("Applying device policy {} to device {}", target, id);
    Pointer<LinuxDevice> device = std::static_pointer_cast<LinuxDevice>(getDevice(id));
    std::unique_lock<std::mutex> device_lock(device->refDeviceMutex());

    sysioApplyTarget(device->getSysPath(), target);
    device->setTarget(target);

    return std::move(device);
  }

  void LinuxDeviceManager::sysioApplyTarget(const String& sys_path, Rule::Target target)
  {
    const char *target_file = nullptr;
    int target_value = 0;

    switch (target)
      {
      case Rule::Target::Allow:
	target_file = "authorized";
	target_value = 1;
	break;
      case Rule::Target::Block:
	target_file = "authorized";
	target_value = 0;
	break;
      case Rule::Target::Reject:
	target_file = "remove";
	target_value = 1;
	break;
      default:
	//log->critical("BUG: unknown rule target");
	throw std::runtime_error("Unknown rule target in applyDevicePolicy");
      }

    char sysio_path[SYSIO_PATH_MAX];
    snprintf(sysio_path, SYSIO_PATH_MAX, "%s/%s", sys_path.c_str(), target_file);
    /* FIXME: check that snprintf wrote the whole path */
    //log->debug("SysIO: writing '{}' to {}", target_value, sysio_path);
    sysioWrite(sysio_path, target_value);
    return;
  }

  void LinuxDeviceManager::thread()
  {
    //log->debug("Entering LinuxDeviceManager thread");

    const int umon_fd = udev_monitor_get_fd(_umon);
    const int max_fd = umon_fd > _event_fd ? umon_fd : _event_fd;
    fd_set readset;

    udev_monitor_enable_receiving(_umon);
    udevEnumerateDevices(); /* scan() without thread state check */

    while (!_thread.stopRequested()) {
      struct timeval tv_timeout = { 5, 0 };
      
      FD_ZERO(&readset);
      FD_SET(umon_fd, &readset);
      FD_SET(_event_fd, &readset);
      
      switch (select(max_fd + 1, &readset, NULL, NULL, &tv_timeout)) {
      case 1: /* Device or event */
      case 2: /* Device and event */
	if (FD_ISSET(_event_fd, &readset)) {
	  //log->debug("Wakeup event received");
	  continue;
	}
	if (FD_ISSET(umon_fd, &readset)) {
	  //log->debug("Handling UDev read event");
	  udevReceiveDevice();
	}
	break;
      case 0: /* Timeout */
	continue;
      case -1: /* Error */
      default:
	//log->debug("Select failure: {}", errno);
	_thread.stop();
      }
    } /* Thread main loop */
    //log->debug("Returning from LinuxDeviceManager thread");
    return;
  }

  void LinuxDeviceManager::udevReceiveDevice()
  {
    struct udev_device *dev = udev_monitor_receive_device(_umon);

    if (!dev) {
      return;
    }

    const char *action_cstr = udev_device_get_action(dev);

    if (!action_cstr) {
      //log->warn("BUG? Device event witout action value.");
      udev_device_unref(dev);
      return;
    }

    if (strcmp(action_cstr, "add") == 0) {
      processDeviceInsertion(dev);
    }
    else if (strcmp(action_cstr, "remove") == 0) {
      processDeviceRemoval(dev);
    }
    else {
      //log->warn("BUG? Unknown device action value \"{}\"", action_cstr);
    }

    udev_device_unref(dev);
    return;
  }

  void LinuxDeviceManager::udevEnumerateDevices()
  {
    struct udev_enumerate *enumerate = udev_enumerate_new(_udev);

    if (enumerate == nullptr) {
      //log->debug("udev_enumerate_new returned NULL");
      throw std::runtime_error("udev_enumerate_new returned NULL");
    }

    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *dlentry = nullptr;

    udev_list_entry_foreach(dlentry, devices) {
      const char *syspath = udev_list_entry_get_name(dlentry);

      if (syspath == nullptr) {
	//log->warn("Received NULL syspath for en UDev enumerated device. Ignoring.");
	continue;
      }

      struct udev_device *device = udev_device_new_from_syspath(_udev, syspath);

      if (device == nullptr) {
	//log->warn("Cannot create a new device from syspath {}. Ignoring.", syspath);
	continue;
      }

      const char *devtype = udev_device_get_devtype(device);

      if (devtype == nullptr) {
	//log->warn("Cannot get device type for device at syspath {}. Ignoring.", syspath);
	udev_device_unref(device);
	continue;
      }

      if (strcmp(devtype, "usb_device") == 0) {
	processDevicePresence(device);
      }

      udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);
    return;
  }

  void LinuxDeviceManager::processDevicePresence(struct udev_device *dev)
  {
    const String sys_path(udev_device_get_syspath(dev));
    try {
      Pointer<LinuxDevice> device = makePointer<LinuxDevice>(*this, dev);
      insertDevice(device);
      DevicePresent(device);
      return;
    }
    catch(const std::exception& ex) {
      logger->error("Exception caught during device presence processing: {}: {}", sys_path, ex.what());
    }
    catch(...) {
      logger->error("Unknown exception while processing device: {}", sys_path);
    }
    /*
     * We don't reject the device here (as is done in processDeviceInsertion)
     * because the device was already connected to the system when USBGuard
     * started. Therefore, if the device is malicious, it already had a chance
     * to interact with the system.
     */
    return;
  }

  void LinuxDeviceManager::processDeviceInsertion(struct udev_device *dev)
  {
    const String sys_path(udev_device_get_syspath(dev));
    try {
      Pointer<LinuxDevice> device = makePointer<LinuxDevice>(*this, dev);
      insertDevice(device);
      DeviceInserted(device);
      return;
    }
    catch(const std::exception& ex) {
      logger->error("Exception caught during device insertion processing: {}: {}", sys_path, ex.what());
    }
    catch(...) {
      logger->error("Unknown exception while processing device: {}", sys_path);
    }

    /*
     * Something went wrong and an exception was generated.
     * Either the device is malicious or the system lacks some
     * resources to successfully process the device. In either
     * case, we take the safe route and fallback to rejecting
     * the device.
     */
    sysioApplyTarget(sys_path, Rule::Target::Reject);

    return;
  }

  void LinuxDeviceManager::insertDevice(Pointer<Device> device)
  {
    DeviceManager::insertDevice(device);
    std::unique_lock<std::mutex> device_lock(device->refDeviceMutex());
    _syspath_map[std::static_pointer_cast<LinuxDevice>(device)->getSysPath()] = device->getID();
    return;
  }

  void LinuxDeviceManager::processDeviceRemoval(struct udev_device *dev)
  {
    //log->debug("Processing device removal");
    const char *syspath_cstr = udev_device_get_syspath(dev);
    if (!syspath_cstr) {
      return;
    }
    const String syspath(syspath_cstr);
    try {
      Pointer<Device> device = removeDevice(syspath);
      DeviceRemoved(device);
    } catch(...) {
      /* Ignore for now */
      //log->debug("Removal of an unknown device ignored.");
      return;
    }
    return;
  }

  Pointer<Device> LinuxDeviceManager::removeDevice(const String& syspath)
  {
    /* FIXME: device map locking */
    auto it = _syspath_map.find(syspath);
    if (it == _syspath_map.end()) {
      throw std::runtime_error("Unknown device, cannot remove from syspath map");
    }
    const uint32_t id = it->second;
    Pointer<Device> device = DeviceManager::removeDevice(id);
    _syspath_map.erase(it);
    return device;
  }

  uint32_t LinuxDeviceManager::getIDFromSysPath(const String& syspath) const
  {
    return _syspath_map.at(syspath);
  }
} /* namespace usbguard */
