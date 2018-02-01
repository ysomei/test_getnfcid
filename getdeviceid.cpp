/* usb test with libusb */
#include <iostream>
#include <unistd.h>
#include "libusb.h"

#define PRODUCT_ID 0x06C3  // RC-S380
#define VENDOR_ID 0x054C   // SONY

#define DATASIZE 255
#define TIMEOUT 5000

// ---------------------------------------------------------------------------
struct device_info {
  libusb_device *dev;
  libusb_device_handle *dh;
  uint8_t ep_in;
  uint8_t ep_out;
  int interface_num;
};
typedef device_info usb_device_info;

// ---------------------------------------------------------------------------
usb_device_info *get_usb_information(libusb_device_handle *dh) {
  usb_device_info *devinfo;
  devinfo = (usb_device_info *)malloc(sizeof(usb_device_info));
  if(devinfo == NULL) return NULL;
  memset(devinfo, 0, sizeof(usb_device_info));
  
  libusb_device *dev;
  struct libusb_config_descriptor *conf;
  const struct libusb_endpoint_descriptor *endp;
  const struct libusb_interface *intf;
  const struct libusb_interface_descriptor *intdesc;
  int ret;
  
  dev = libusb_get_device(dh);
  if(dev == NULL){
    std::cout << "device get error..." << std::endl;
    return NULL;
  }
  devinfo->dh = dh;
  devinfo->dev = dev;
  
  libusb_get_config_descriptor(dev, 0, &conf);  
  for(int i = 0; i < (int)conf->bNumInterfaces; i++){
    intf = &conf->interface[i];
    for(int j = 0; j < intf->num_altsetting; j++){
      intdesc = &intf->altsetting[j];
      for(int k = 0; k < (int)intdesc->bNumEndpoints; k++){
    endp = &intdesc->endpoint[k];
    
    switch(endp->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) {
    case LIBUSB_TRANSFER_TYPE_BULK:
      //printf("bulk endpoint: %02x\n", endp->bEndpointAddress);
      if((endp->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN){
        devinfo->ep_in = endp->bEndpointAddress;
      }
      if((endp->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT){
        devinfo->ep_out = endp->bEndpointAddress;
      }
      break;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
      //printf("interrupt endpoint: %02x\n", endp->bEndpointAddress);
      break;
    }
      }
    }    
  }
  libusb_free_config_descriptor(conf);
  
  return devinfo;
}

// ----------------------------------------------------------------------------
void show_data(uint8_t *buf, int size) {
  for(int i = 0; i != size; i++){
    printf("%02x", buf[i]);
  }
  printf("\n");  
}

// --------------------------------------------------------------------------
uint8_t *packet_send(usb_device_info *devinfo, uint8_t *buf, int size) {
  uint8_t rcv[255], rbuf[255];
  int len;
  int ret;
  
  //printf("send -> ");
  //show_data(buf, size);

  ret = libusb_bulk_transfer(devinfo->dh, devinfo->ep_out,
                 (unsigned char *)buf, size, &len, TIMEOUT);
  if(ret < 0){
    std::cout << "data send error..." << std::endl;
    return NULL;
  }

  // receive ack/nck
  ret = libusb_bulk_transfer(devinfo->dh, devinfo->ep_in,
              (unsigned char *)rcv, sizeof(rcv), &len, TIMEOUT);
  if(ret < 0){
    std::cout << "data receive error..." << std::endl;
    return NULL;
  }
  //printf("recv <- ");
  //show_data(rcv, len);

  // receive response
  ret = libusb_bulk_transfer(devinfo->dh, devinfo->ep_in,
                     (unsigned char *)rbuf, sizeof(rbuf), &len, TIMEOUT);
  if(ret < 0){
    std::cout << "data receive error..." << std::endl;
    return NULL;
  }
  //printf("recv <- ");
  //show_data(rbuf, len);

  uint8_t *rb;
  rb = new uint8_t[len];
  memcpy(rb, rbuf, len);
  return rb;
}

// ---------------------------------------------------------------------------
short checksum(char cmd, uint8_t *buf, int size) {
  int sum = (unsigned int)cmd;
  for(int i = 0; i < size; i++){
    sum += buf[i];
  }
  return (0x100 - sum) % 0x100; 
}

// ---------------------------------------------------------------------------
uint8_t *packet_write(usb_device_info *devinfo, uint8_t *buf, int size) {
  uint8_t cmd[DATASIZE];
  int n;
  short csum;
  int ret;

  n = size;
  if(n < 1) return 0;

  // data = 0xd6 + data
  // len = len(data)
  // 00 00 ff ff ff len(L) len(H) checksum(len) data checksum(data) 00
  cmd[0] = 0x00; cmd[1] = 0x00; cmd[2] = 0xff;
  cmd[3] = 0xff; cmd[4] = 0xff; 
  cmd[5] = ((n + 1) & 0xff) ; cmd[6] = ((n + 1) & 0xff00) >> 8;
  csum = (0x100 - (cmd[5] + cmd[6])) % 0x100;
  cmd[7] = csum;

  cmd[8] = 0xd6;
  memcpy(cmd + 9, buf, size);

  csum = checksum(cmd[8], buf, size);
  cmd[9 + n] = csum;
  
  cmd[10 + n] = 0x00;
  n += 11;
  
  return packet_send(devinfo, cmd, n);
}

uint8_t *packet_parse(uint8_t *buf, int size) {
  uint8_t *rb;
  rb = new uint8_t[size - 10];
  memcpy(rb, buf + 9, size - 10);
  return rb;
}

// ---------------------------------------------------------------------------
int packet_init(usb_device_info *devinfo) {
  uint8_t cmd[6];
  int ret;
  int len;

  // ack command
  memcpy(cmd, "\x00\x00\xff\x00\xff\x00", 6);
  //printf("send -> ");
  //show_data(cmd, sizeof(cmd));
  
  ret = libusb_bulk_transfer(devinfo->dh, devinfo->ep_out,
                 (unsigned char *)cmd, sizeof(cmd), &len, TIMEOUT);
  if(ret < 0) std::cout << "data send error..." << std::endl;
  return ret;
}

uint8_t *packet_setcommandtype(usb_device_info *devinfo) {
  uint8_t cmd[2];
  memcpy(cmd, "\x2a\x01", 2);
  return packet_write(devinfo, cmd, sizeof(cmd));
}

uint8_t *packet_switch_rf(usb_device_info *devinfo) {
  uint8_t cmd[2];
  memcpy(cmd, "\x06\x00", 2);
  return packet_write(devinfo, cmd, sizeof(cmd));
}

uint8_t *packet_inset_rf(usb_device_info *devinfo, char type) {
  uint8_t cmd[5];
  if(type == 'F') memcpy(cmd, "\x00\x01\x01\x0f\x01", 5); // 212F
  if(type == 'A') memcpy(cmd, "\x00\x02\x03\x0f\x03", 5); // 106A
  if(type == 'B') memcpy(cmd, "\x00\x03\x07\x0f\x07", 5); // 106B
  return packet_write(devinfo, cmd, sizeof(cmd));  
}

uint8_t *packet_inset_protocol_1(usb_device_info *devinfo) {
  uint8_t cmd[39];
  memcpy(cmd, "\x02\x00\x18\x01\x01\x02\x01\x03\x00\x04\x00\x05\x00\x06\x00\x07\x08\x08\x00\x09\x00\x0a\x00\x0b\x00\x0c\x00\x0e\x04\x0f\x00\x10\x00\x11\x00\x12\x00\x13\x06", 39);
  return packet_write(devinfo, cmd, sizeof(cmd));  
}

uint8_t *packet_inset_protocol_2(usb_device_info *devinfo, char type) {
  uint8_t cmd[11];
  int len;
  if(type == 'F'){
    len = 3;
    memcpy(cmd, "\x02\x00\x18", len);
  }
  if(type == 'A'){
    len = 11;
    memcpy(cmd, "\x02\x00\x06\x01\x00\x02\x00\x05\x01\x07\x07", len);
  }
  if(type == 'B'){
    len = 11;
    memcpy(cmd, "\x02\x00\x14\x09\x01\x0a\x01\x0b\x01\x0c\x01", len);
  }
  return packet_write(devinfo, cmd, len);  
}

uint8_t *packet_sens_req(usb_device_info *devinfo, char type) {
  uint8_t cmd[9];
  int len;
  if(type == 'F'){
    len = 9;
    memcpy(cmd, "\x04\x6e\x00\x06\x00\xff\xff\x01\x00", len);
  }
  if(type == 'A'){
    len = 4;
    memcpy(cmd, "\x04\x6e\x00\x26", len);
  }
  if(type == 'B'){
    len = 6;
    memcpy(cmd, "\x04\x6e\x00\x05\x00\x10", len);
  }
  return packet_write(devinfo, cmd, len);  
}

// ---------------------------------------------------------------------------
// usage: usbtest [-F | -B]  default target is Type-F(FeliCa)
int main(int argc, char *argv[]) {

  libusb_device_handle *dh = NULL;
  usb_device_info *devinfo;
  uint8_t *rbuf;
  int ret;
  int rlen, rcmd;
  bool isloop = true;
  char nfc_type;

  uint8_t idm[8], pmm[8];
  uint8_t nfcid[4], appdata[4], pinfo[4];
  
  nfc_type = 'F'; // default target type FeliCa
  if(argc == 2){
    nfc_type = argv[1][0];
    if(argv[1][0] == '-') nfc_type = argv[1][1];

    if(nfc_type != 'F' && nfc_type != 'B'){
      std::cout << "usage: " << argv[0] << " [-F | -B]" << std::endl;
      exit(1);
    }
  }
  std::cout << "NFC Type-" << nfc_type << " scanning..." << std::endl;
    
  // open
  libusb_init(NULL);
  dh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
  if(dh != NULL){ 
    // usb interface setting
    libusb_set_auto_detach_kernel_driver(dh, 1);
    libusb_set_configuration(dh, 1);
    libusb_claim_interface(dh, 0);
    libusb_set_interface_alt_setting(dh, 0, 0);

    // get usb information
    devinfo = get_usb_information(dh);
    
    // init
    packet_init(devinfo);
    packet_setcommandtype(devinfo);
    packet_switch_rf(devinfo);
    
    packet_inset_rf(devinfo, nfc_type);
    packet_inset_protocol_1(devinfo);    
    packet_inset_protocol_2(devinfo, nfc_type);
    
    while (isloop){      
      rbuf = packet_sens_req(devinfo, nfc_type);
      if(rbuf[9] == 0x05 && rbuf[10] == 0x00){
    rlen = ((rbuf[6] << 8) + rbuf[5]);
    rbuf = packet_parse(rbuf, rlen);

    // Type-F
    if(rbuf[6] == 0x14 && rbuf[7] == 0x01){
      memcpy(idm, rbuf + 8, 8);
      memcpy(pmm, rbuf + 16, 8);

      printf(" IDm: "); show_data(idm, 8);
      printf(" PMm: "); show_data(pmm, 8);
    }
    // Type-B
    if(rbuf[6] == 0x50){
      memcpy(nfcid, rbuf + 7, 4);
      memcpy(appdata, rbuf + 11, 4);
      memcpy(pinfo, rbuf + 15, 4);

      printf(" NFCID: "); show_data(nfcid, 4);
      printf(" Application Data: "); show_data(appdata, 4);
      printf(" Protocol Info: "); show_data(pinfo, 4);
    }   
    isloop = false;
      }
      usleep(250 * 1000);
    }
    
    // close
    libusb_release_interface(dh, 0);
    libusb_close(dh);
  } else {
    printf("Can not open device %04x/%04x\n", VENDOR_ID, PRODUCT_ID);
  }
  libusb_exit(NULL);

  return 0;
}
