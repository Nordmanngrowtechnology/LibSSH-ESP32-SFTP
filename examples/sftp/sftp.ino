
// Set local WiFi credentials below.
const char *configSTASSID = "YourWiFiSSID";
const char *configSTAPSK = "YourWiFiPSK";

#include "WiFi.h"
#include "libssh_esp32.h"

// Based on https://api.libssh.org/stable/libssh_tutor_sftp.html

// Opening and closing a SFTP session
int sftp_open_closing(ssh_session session)
{
    sftp_session sftp;
    int rc;

    sftp = sftp_new(session);
    if (sftp == NULL)
    {
        fprintf(stderr, "Error allocating SFTP session: %s\n",
                ssh_get_error(session));
        return SSH_ERROR;
    }

    rc = sftp_init(sftp);
    if (rc != SSH_OK)
    {
        fprintf(stderr, "Error initializing SFTP session: code %d.\n",
                sftp_get_error(sftp));
        sftp_free(sftp);
        return rc;
    }

    ...

    sftp_free(sftp);
    return SSH_OK;
}

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
    Serial.begin(115200);
}

void loop()
{
}