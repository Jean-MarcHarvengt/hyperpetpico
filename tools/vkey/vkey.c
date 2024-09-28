#include <stdio.h>
#include <ncurses.h>
#include <libusb.h>
#include <memory.h>
#include <unistd.h>
#include <signal.h>

#define ctrl(x)           ((x) & 0x1f)

#define USB_TIMEOUT 200
#define MAX_PACKET  64
#define CMD_MAXSIZE (MAX_PACKET-2)

#define HYP_VENDOR_ID 0x0000
#define HYP_PRODUCT_ID 0x0001

#define USB_DIR_OUT 0x00u
#define USB_DIR_IN 0x80u

#define MAX_FILESIZE 0x8000

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static bool kb_mode = true;
static char cmdbuffer[CMD_MAXSIZE+1];
static int cmdpt=0;
static int y_cmdline;
static unsigned char filebuffer[MAX_FILESIZE];

static int epinadd;
static int epoutadd;
static int epinmax;
static int epoutmax;

static struct libusb_device_handle *usb_dev;
static unsigned char txbuffer[MAX_PACKET];
static unsigned char rxbuffer[MAX_PACKET];

typedef enum {
  sercmd_undef=0,
  sercmd_reset=1,
  sercmd_key=2,
  sercmd_prg=3,
  sercmd_run=4,
} SerialCmd;

// Ctrl+Z
void sigstop(int p){

}
// Ctrl+C
void sigkill(int p){
}

static int send_packet(unsigned char * buf, int buflen) {
    int bytes_sent = 0;
    int result = libusb_bulk_transfer( usb_dev, epoutadd, buf, buflen, &bytes_sent, USB_TIMEOUT );
//    printf( "sent %d bytes\n",bytes_sent);
//    int result = libusb_interrupt_transfer( usb_dev, epoutadd, buf, buflen, &bytes_sent, USB_TIMEOUT );
    if ( result < 0 ) // Was the interrupt transfer successful?
    {
        printf( "error sending interrupt transfer: %s\n", libusb_error_name( result ) );
    }
    return result;
}

static int receive_packet(void) {
    int bytes_read = 0;

    int result = libusb_bulk_transfer( usb_dev, epinadd, rxbuffer, MAX_PACKET, &bytes_read, USB_TIMEOUT );
//    int result = libusb_interrupt_transfer( usb_dev, epinadd, rxbuffer, MAX_PACKET, &bytes_read, USB_TIMEOUT );
//    printf( "read %d bytes\n",bytes_read);
    if ( result < 0 ) // Was the interrupt transfer successful?
    {
        printf( "Error reading interrupt transfer: %s\n", libusb_error_name( result ) );
        return result;
    }
    else {
        rxbuffer[bytes_read] = 0;
//        printf( "received %s\n",rxbuffer);
        return bytes_read;
    }
}


static void run_command(char * cmd) {
    cmdbuffer[cmdpt] = 0;
    //printw("%s", cmd);
    if (!strcmp(cmd,"reset") ) 
    {
        unsigned char c = sercmd_reset;
        send_packet(&c, 1); 
        receive_packet();
    } 
    else if (!strcmp(cmd,"run") ) 
    {
        unsigned char c = sercmd_run;
        send_packet(&c, 1); 
        receive_packet();
   }
    else if (!strncmp(cmd,"load ",5) ) 
    {
        if (strlen(cmd) > 5) {
            FILE *fp_rd;
            if ((fp_rd = fopen (&cmd[5], "rb")) == NULL)
            {
                move(y_cmdline-2, 0);
                clrtoeol();               
                printw("can not open program %s\n", &cmd[5]);
                return;      
            } 
      
            fseek(fp_rd, 0L, SEEK_END);
            int size = ftell(fp_rd);
            fseek(fp_rd, 0L, SEEK_SET);
            if (size > MAX_FILESIZE) {
                move(y_cmdline-2, 0);
                clrtoeol();               
                printw("program %s is too big!\n", &cmd[5]);
                fclose(fp_rd);
                return;
            }

            if (fread(&filebuffer[0], 1, size, fp_rd) != size) {
                move(y_cmdline-2, 0);
                clrtoeol();               
                printw("error reading program %s\n", &cmd[5]);
                fclose(fp_rd);
                return;
            }
            fclose(fp_rd);
            move(y_cmdline-2, 0);
            clrtoeol();               
            printw("sending program %s, %d bytes\n", &cmd[5], size);
            int remaining = size;
            int fpos = 0;
            while (remaining > 0) {
                int tosend = MIN(MAX_PACKET-1,remaining);
                memcpy((void*)&txbuffer[1],(void*)&filebuffer[fpos],tosend);
                txbuffer[0] = sercmd_prg;
                send_packet(&txbuffer[0], tosend+1); 
                receive_packet();
                move(y_cmdline-2, 0);
                clrtoeol();               
                printw("sent %d bytes\n", fpos);
                refresh();
                fpos += tosend;
                remaining -= tosend;
            }
            move(y_cmdline-2, 0);
            clrtoeol();               
            printw("program sent\n");
        }
    }
    else 
    {
        move(y_cmdline-2, 0);
        clrtoeol();               
        printw("invalid command\n");
    }    
}

static void send_key(char key) {
    unsigned char c[2] = { sercmd_key,key };
    send_packet(&c[0], 2); 
    receive_packet();
}


int main(void) {
    struct libusb_context *context;

    if (libusb_init(&context) < 0) {
        printf("could not initialize libusb...\n");
        return 0;
    }  
    libusb_device **list = NULL;
    size_t count = libusb_get_device_list(context, &list);
    bool found = false;
    libusb_device *dev = NULL;
    size_t idx;
    for (idx = 0; idx < count; ++idx) 
    {
        dev = list[idx];
        struct libusb_device_descriptor desc= {0};
        libusb_get_device_descriptor(dev, &desc);
        printf("Vendor:Device = %04x:%04x\n", desc.idVendor, desc.idProduct);
        if ( (desc.idVendor == HYP_VENDOR_ID) && (desc.idProduct == HYP_PRODUCT_ID) ) {
            printf("found hyperpet pico...\n");
            found = true;
            break;
        }    

    }
    libusb_free_device_list(list, 1);    
    if (!found) {
        printf("Cound not found hyperpet pico on USB...\n");
        libusb_exit(context);        
        return -1;
    }    

    struct libusb_config_descriptor *config0;
    if (libusb_get_config_descriptor(dev, 0, &config0) < 0) {
        printf("Could not retrieve config descriptor...\n");
        libusb_exit(context);        
        return -1;
    } 



    struct libusb_interface interface = config0->interface[0];
    if (interface.num_altsetting > 0)
    {
        struct libusb_interface_descriptor descriptor = interface.altsetting[0];
        printf("bNumEndpoints %d\r\n",descriptor.bNumEndpoints);
        if (descriptor.bNumEndpoints != 2)
        {
            printf("unexpected NumEndpoints...\n");            
            libusb_free_config_descriptor (config0);
            libusb_exit(context);        
            return -1;
        }
        printf("wMaxPacketSize 0 %d\r\n",descriptor.endpoint[0].wMaxPacketSize);
        printf("bEndpointAddress 0 %d\r\n",descriptor.endpoint[0].bEndpointAddress);
        printf("wMaxPacketSize 1 %d\r\n",descriptor.endpoint[1].wMaxPacketSize);
        printf("bEndpointAddress 1 %d\r\n",descriptor.endpoint[1].bEndpointAddress);            
        for (int i=0;i<descriptor.bNumEndpoints;i++)
        {
            if (descriptor.endpoint[i].bEndpointAddress & USB_DIR_IN) {
                epinadd = descriptor.endpoint[i].bEndpointAddress;
                epinmax = descriptor.endpoint[i].wMaxPacketSize;
            }    
            else {
                epoutadd = descriptor.endpoint[i].bEndpointAddress;
                epoutmax = descriptor.endpoint[i].wMaxPacketSize;
            }
        }
    }         
    libusb_free_config_descriptor (config0);

    if (libusb_open(dev, &usb_dev) < 0) {
        printf("Could not open device...\n");
        return -1;
    }

    //usb_dev = libusb_open_device_with_vid_pid (context, HYP_VENDOR_ID, HYP_PRODUCT_ID);


    if (libusb_get_config_descriptor(dev, 0, &config0) < 0) {
        printf("Could not get config descriptor...\n");
        return -1;
    }

    int cfg;
    if (libusb_get_configuration (usb_dev, &cfg) < 0) {
        printf("Could not get config...\n");
        return -1;
    }

    int cfg0 = config0->bConfigurationValue;
    if (cfg != cfg0)
    {
        printf("Setting config...\n");
        if (libusb_set_configuration(usb_dev, cfg0) < 0) {
            printf("Could not set config...\n");
            return -1;
        }
     }

    if (libusb_set_auto_detach_kernel_driver(usb_dev, 1) != LIBUSB_SUCCESS)  {
        printf("Could not detach kernel...\n");
        libusb_close( usb_dev );
        libusb_exit(context);
        return -1;
    }

    if (libusb_claim_interface( usb_dev, 0 )) {
        printf("Could not clain interface 0...\n");
        libusb_close( usb_dev );
        libusb_exit(context);        
        return -1;
    }


    //run_command("reset");

 
    //send_packet((unsigned char *)"Hello World!", strlen("Hello World!"));
    //send_packet((unsigned char *)"Hello World!", strlen("Hello World!"));
    //receive_packet();
    //send_packet((unsigned char *)"Hello", strlen("Hello"));
    //receive_packet();

/*
    FILE *fp_rd;
//    fp_rd = fopen ("back2pet.prg", "rb");      
    fp_rd = fopen ("bitmap1.prg", "rb");      
    fseek(fp_rd, 0L, SEEK_END);
    int size = ftell(fp_rd);
    fseek(fp_rd, 0L, SEEK_SET);
    fread(&filebuffer[0], 1, size, fp_rd);
               
    printf("sending file %d bytes\n", size);
    int remaining = size;
    int fpos = 0;
    
    while (remaining > 0) {
                int tosend = MIN(MAX_PACKET-1,remaining);
        memcpy((void*)&txbuffer[1],(void*)&filebuffer[fpos],tosend);
        txbuffer[0] = sercmd_prg;
        send_packet(&txbuffer[0], tosend+1); 
        receive_packet();
        printf("sent %d bytes\n", fpos);
        fpos += tosend;
        remaining -= tosend;
    }
    printf("file sent\n");
    fclose(fp_rd);
    unsigned char c = sercmd_run;
    send_packet(&c, 1); 
    receive_packet();
    libusb_release_interface( usb_dev, 0 );

    libusb_close( usb_dev );
    libusb_exit(context);

    return 0 ;
*/

    initscr();
    printw("Usage:\n");
    printw("ctrl-x/e   = exit\n");
    printw("ctrl-f/k   = file/keyboard mode toggle\n");
    printw("\n");
    printw("File mode commands:\n");
    printw("  load <filename> = inject program\n");
    printw("  run             = run program\n");
    printw("  reset           = reset system\n");
    printw("\n");
    printw("Default is keyboard mode.\n");
    printw("\n");
    printw("(now sending keys to hyperpetpico)\n");

    signal(SIGTSTP,&sigstop);      //for Ctrl+Z
    signal(SIGINT,&sigkill);       //for Ctrl+C

    int x;
    getyx(stdscr, y_cmdline, x); // save current pos
    int l = -1;
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    bool exit=false;
    while (!exit) {
        int c = getch();
        //printf("%d",c );
        switch (c) {
            case 0x7f:
                if (!kb_mode) {
                    if (cmdpt>0) {
                        cmdpt--;
                        cmdbuffer[cmdpt] = 0;
                    } 
                    move(y_cmdline, 0);
                    clrtoeol();
                    printw("%s",&cmdbuffer[0]);   
                }
                break;

            case 0xd:
                if (!kb_mode) {
                    move(y_cmdline-2, 0);
                    clrtoeol();
                    move(y_cmdline, 0);
                    clrtoeol();
                    run_command(cmdbuffer);
                    cmdpt=0;
                    move(y_cmdline, 0);
                    clrtoeol();
                }
                else {
                    send_key(0xd);
                }
                break;
            case ctrl('x'):
            case ctrl('e'):                
                exit = true;
                break;
            case ctrl('f'):
            case ctrl('k'):
                move(y_cmdline-2, 0);
                clrtoeol();                      
                move(y_cmdline-1, 0);
                clrtoeol();
                if (kb_mode) {
                    // file mode
                    kb_mode = false;
                    cmdpt=0;                    
                    printw("(file mode: enter command)\n");
                }
                else  {
                    kb_mode = true;
                    printw("(now sending keys to hyperpetpico)\n");
                }
                move(y_cmdline, 0);
                clrtoeol();
                break;
            case ctrl('a'):
            case ctrl('r'):
            case ctrl('t'):
            case ctrl('v'):
                break;
            default:
                if ( (c < 0x80) /*&& (ctrl(c) == c)*/ ) {  
                    if (kb_mode) {
                        // send key to pet
                        send_key(c);
                    }
                    else  {
                        printw("%c", c);
                        if (cmdpt < CMD_MAXSIZE) {
                            cmdbuffer[cmdpt++] = c;
                        }
                        else {
                            run_command(cmdbuffer);
                            cmdpt=0;
                        }
                    }
                 }     
                break;
        }
    }

    endwin();

    libusb_release_interface( usb_dev, 0 );
    libusb_close( usb_dev );
    libusb_exit(context);
    printf("exiting vkey...\n");    
    return 1;
}