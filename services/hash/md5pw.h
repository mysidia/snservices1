#ifdef __cplusplus
extern "C" {
#else
typedef unsigned char bool;
#endif
struct auth_data;

unsigned char *md5_unhex(unsigned char str[512]);
unsigned char *md5_password(unsigned char *pw);
void pw_enter_password(char *, unsigned char *, char);
unsigned char *plaintext_password(char *pw);
char *md5_printable(unsigned char md5buffer[16]);
void md5_buffer(unsigned char *passwd, int len, unsigned char *target);
bool Valid_pw(char *argument, unsigned char *password, char);
int Valid_md5key( char* key, struct auth_data* auth, const char* to_object, unsigned char* passwd, char enc );
int isMD5Key(const char* password);

/*! Information structure for IDENTIFY-MD5 data */
struct auth_data
{
          u_int32_t auth_cookie;
          u_int16_t auth_user;
          int   format;
};

#ifdef __cplusplus
}
#endif
