#define KBD_FLAG_SHIFT  0x01
#define KBD_FLAG_CONTROL  0x02
#define KBD_FLAG_ALT  0x04

#define KBD_KEY_BS 0x14 //8
#define KBD_KEY_ENTER 13
#define KBD_KEY_DOWN 0x11 //1000
#define KBD_KEY_UP 1001
#define KBD_KEY_PGDN 1002
#define KBD_KEY_PGUP 1003
#define KBD_KEY_RIGHT 1004
#define KBD_KEY_LEFT 0xDF //1005
#define KBD_KEY_HOME 1006
#define KBD_KEY_END 1007

#define KEY_PRESSED  1
#define KEY_RELEASED 0


#ifdef __cplusplus
extern "C" {
#endif

/* raw_key_down should be called whenever a key is pressed, anywhere.
 * The code should distinguish upper and lower case letters, and
 * symbols that appear on the same key, but no other processing.
 * For example, ctrl-A is 'A' with a flag to indicate that ctrl
 * is pressed. */
extern void kbd_signal_raw_key(int code, int flags, int pressed);

#ifdef __cplusplus
}
#endif