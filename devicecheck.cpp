/* usb test with libusb-1.0 */
#include <iostream>
#include "libusb.h"

int main() {
  libusb_device **devs;
  libusb_device *dev;
  libusb_device_descriptor desc;
  libusb_device_handle *dh;

  uint8_t path[8];
  uint8_t sdat[255];
  int r, cnt;

  r = libusb_init(NULL); // initalize
  if(r < 0) return r;

  cnt = libusb_get_device_list(NULL, &devs);
  if(cnt < 0) return cnt;

  for(int i = 0; i < cnt; i++){
    dev = devs[i];    
    r = libusb_get_device_descriptor(dev, &desc);
    if(r < 0){
      std::cout << "device get error..." << std::endl;
      return r;
    }

    // show device description
    printf("%04x/%04x (bus %d, device %d)", desc.idVendor, desc.idProduct,
       libusb_get_bus_number(dev), libusb_get_device_address(dev));

    r = libusb_get_port_numbers(dev, path, sizeof(path));
    if(r < 0) return r;
    printf(" path: %d", path[0]);
    for(int j = 1; j < r; j++){
      printf(".%d", path[j]);
    }

    // show spec string
    libusb_open(dev, &dh);
    // manufacturer, product, serialnumber 
    r = libusb_get_string_descriptor_ascii(dh, desc.iManufacturer,
                       (unsigned char *)sdat, sizeof(sdat));
    if(r > -1) printf(" %s", sdat);
    r = libusb_get_string_descriptor_ascii(dh, desc.iProduct,
                       (unsigned char *)sdat, sizeof(sdat));
    if(r > -1) printf(" %s", sdat);
    r = libusb_get_string_descriptor_ascii(dh, desc.iSerialNumber,
                       (unsigned char *)sdat, sizeof(sdat));
    if(r > -1) printf(" %s", sdat);
    
    libusb_release_interface(dh, 0);
    libusb_close(dh);

    printf("\n");
  }
  libusb_free_device_list(devs, 1);
  
  libusb_exit(NULL); // exit
  return 0;
}
