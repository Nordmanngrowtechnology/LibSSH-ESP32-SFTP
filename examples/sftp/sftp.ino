#include <Arduino.h>
#include <WiFi.h>

#define USE_SERIAL Serial

#include "libssh_esp32.h" // inti the lib with libssh_begin() call construtor and auto _ssh_init
#include <libssh/libssh.h> // << add the main function
//#include <libssh/sftp.h> // << add sftp



// Set local WiFi credentials below.
wifi WiFi;
const char *ssid = "YourWiFiSSID";
const char *password = "YourWiFiPSK";

// SSH connection
const char *host = "localhost";
const char *user = "user_name";
int verbosity = 1;

ssh_session connect_ssh(const char *host, const char *user,int verbosity){
  ssh_session session;
  int auth=0;

  session=ssh_new();
  if (session == NULL) {
    return NULL;
  }

  if(user != NULL){
    if (ssh_options_set(session, SSH_OPTIONS_USER, user) < 0) {
      ssh_free(session);
      return NULL;
    }
  }

  if (ssh_options_set(session, SSH_OPTIONS_HOST, host) < 0) {
    ssh_free(session);
    return NULL;
  }
  ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
  if(ssh_connect(session)){
    fprintf(stderr,"Connection failed : %s\n",ssh_get_error(session));
    ssh_disconnect(session);
    ssh_free(session);
    return NULL;
  }
  if(verify_knownhost(session)<0){
    ssh_disconnect(session);
    ssh_free(session);
    return NULL;
  }
  auth=authenticate_console(session);
  if(auth==SSH_AUTH_SUCCESS){
    return session;
  } else if(auth==SSH_AUTH_DENIED){
    fprintf(stderr,"Authentication failed\n");
  } else {
    fprintf(stderr,"Error while authenticating : %s\n",ssh_get_error(session));
  }
  ssh_disconnect(session);
  ssh_free(session);
  return NULL;
}

// open SFTP session
#define DATALEN 65536
static void do_sftp(ssh_session session){
    sftp_session sftp=sftp_new(session);
    sftp_dir dir;
    sftp_attributes file;
    sftp_statvfs_t sftpstatvfs;
    struct statvfs sysstatvfs;
    sftp_file fichier;
    sftp_file to;
    int len=1;
    unsigned int i;
    char data[DATALEN]={0};
    char *lnk;

    unsigned int count;

    if(!sftp){
        fprintf(stderr, "sftp error initialising channel: %s\n",
            ssh_get_error(session));
        return;
    }
    if(sftp_init(sftp)){
        fprintf(stderr, "error initialising sftp: %s\n",
            ssh_get_error(session));
        return;
    }

    printf("Additional SFTP extensions provided by the server:\n");
    count = sftp_extensions_get_count(sftp);
    for (i = 0; i < count; i++) {
      printf("\t%s, version: %s\n",
          sftp_extensions_get_name(sftp, i),
          sftp_extensions_get_data(sftp, i));
    }


// Based on https://api.libssh.org/stable/libssh_tutor_sftp.html


// Creating a directory
#include <sys/stat.h>
int sftp_creat_directory(ssh_session session, sftp_session sftp)
{
    int rc;

    rc = sftp_mkdir(sftp, "helloworld", S_IRWXU);
    if (rc != SSH_OK)
    {
        if (sftp_get_error(sftp) != SSH_FX_FILE_ALREADY_EXISTS)
        {
            fprintf(stderr, "Can't create directory: %s\n",
                    ssh_get_error(session));
            return rc;
        }
    }

    ...

    return SSH_OK;
}

// Writing to a file on the remote computer
// wirte to a file
#include <sys/stat.h>
#include <fcntl.h>

int sftp_write_tofile(ssh_session session, sftp_session sftp)
{
    int access_type = O_WRONLY | O_CREAT | O_TRUNC;
    sftp_file file;
    const char *helloworld = "Hello, World!\n";
    int length = strlen(helloworld);
    int rc, nwritten;

    ...

    file = sftp_open(sftp, "helloworld/helloworld.txt",
                     access_type, S_IRWXU);
    if (file == NULL)
    {
        fprintf(stderr, "Can't open file for writing: %s\n",
                ssh_get_error(session));
        return SSH_ERROR;
    }

    nwritten = sftp_write(file, helloworld, length);
    if (nwritten != length)
    {
        fprintf(stderr, "Can't write data to file: %s\n",
                ssh_get_error(session));
        sftp_close(file);
        return SSH_ERROR;
    }

    rc = sftp_close(file);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Can't close the written file: %s\n",
                ssh_get_error(session));
        return rc;
    }

    return SSH_OK;
}

// Reading a file from the remote computer
// Good chunk size
#define MAX_XFER_BUF_SIZE 16384

int sftp_read_sync(ssh_session session, sftp_session sftp)
{
    int access_type;
    sftp_file file;
    char buffer[MAX_XFER_BUF_SIZE];
    int nbytes, nwritten, rc;
    int fd;

    access_type = O_RDONLY;
    file = sftp_open(sftp, "/etc/profile",
                     access_type, 0);
    if (file == NULL) {
        fprintf(stderr, "Can't open file for reading: %s\n",
                ssh_get_error(session));
        return SSH_ERROR;
    }

    fd = open("/path/to/profile", O_CREAT);
    if (fd < 0) {
        fprintf(stderr, "Can't open file for writing: %s\n",
                strerror(errno));
        return SSH_ERROR;
    }

    for (;;) {
        nbytes = sftp_read(file, buffer, sizeof(buffer));
        if (nbytes == 0) {
            break; // EOF
        } else if (nbytes < 0) {
            fprintf(stderr, "Error while reading file: %s\n",
                    ssh_get_error(session));
            sftp_close(file);
            return SSH_ERROR;
        }

        nwritten = write(fd, buffer, nbytes);
        if (nwritten != nbytes) {
            fprintf(stderr, "Error writing: %s\n",
                    strerror(errno));
            sftp_close(file);
            return SSH_ERROR;
        }
    }

    rc = sftp_close(file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't close the read file: %s\n",
                ssh_get_error(session));
        return rc;
    }

    return SSH_OK;
}

// Listing the contents of a directory
int sftp_list_dir(ssh_session session, sftp_session sftp)
{
    sftp_dir dir;
    sftp_attributes attributes;
    int rc;

    dir = sftp_opendir(sftp, "/var/log");
    if (!dir)
    {
        fprintf(stderr, "Directory not opened: %s\n",
                ssh_get_error(session));
        return SSH_ERROR;
    }

    printf("Name                       Size Perms    Owner\tGroup\n");

    while ((attributes = sftp_readdir(sftp, dir)) != NULL)
    {
        printf("%-20s %10llu %.8o %s(%d)\t%s(%d)\n",
         attributes->name,
         (long long unsigned int) attributes->size,
         attributes->permissions,
         attributes->owner,
         attributes->uid,
         attributes->group,
         attributes->gid);

        sftp_attributes_free(attributes);
    }

    if (!sftp_dir_eof(dir))
    {
        fprintf(stderr, "Can't list directory: %s\n",
                ssh_get_error(session));
        sftp_closedir(dir);
        return SSH_ERROR;
    }

    rc = sftp_closedir(dir);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Can't close directory: %s\n",
                ssh_get_error(session));
        return rc;
    }
}

void setup()
{
    USE_SERIAL.begin(115200);

     wifi.begin(ssid, password);
     wifi.setSleep(false);

  USE_SERIAL.print("WiFi connecting");
  while (wifi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  USE_SERIAL.println("");
  USE_SERIAL.println("WiFi connected");

  startCameraServer();

  USE_SERIAL.print("IP: ");
  USE_SERIAL.print(wifi.localIP());
  USE_SERIAL.println("' ...");
}
}

void loop()
{
    if ((wifi.status() == WL_CONNECTED)) {
    
    
    // https://github.com/codinn/libssh/blob/master/INSTALL
    //ssh_session session;
    // build ssh session
    session = connect_ssh("localhost", NULL, 0);
    if (session == NULL) {
        ssh_finalize();
        return;
    }

    // init the sftp connection over ssh
    do_sftp(session);
   
    // connect to ftp server with ssh and show dir
    Serial.println("Listing the contents of a directory ");
    //sftp_new(session)



    // close
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();

    return;
    }
    delay(10000);
}