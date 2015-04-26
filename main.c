#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wiringPi.h>

#define IN_SYSTEM_SAVE_FOLDER            "/home/pi/records/onboardSD/"
#define REMOVABLE_DEVICE_SAVE_FOLDER     "/home/pi/records/usb/"

#define MAX_CMD_BUF_LEN                  (1024)

/* use column scan method to get matrix keyboard value */
enum row_io {
    row_0,
    row_1,
    row_2,
    row_3,
    __row_max,
};
static int row[__row_max] = {
    7,    //wiringPi Pin 7, P1-7
    0,    //wiringPi Pin 0, P1-11
    2,    //wiringPi Pin 2, P1-13
    3,    //wiringPi Pin 3, P1-15
};
#define row_mask     (0x81)

enum column_io {
    column_0,
    column_1,
    column_2,
    column_3,
    __column_max,
};
static int column[__column_max] = {
    1,    //wiringPi Pin 1, P1-12
    4,    //wiringPi Pin 4, P1-16
    5,    //wiringPi Pin 5, P1-18
    6,    //wiringPi Pin 6, P1-22
};
#define column_mask   (0x72)

static char matrix_key[__row_max][__column_max] = {
    {'1','2','3','4'},
    {'5','6','7','8'},
    {'9','0','A','B'},
    {'C','D','E','F'},
};

struct record_play {
    int max_filename_len;
    const char* max_record_time;
    bool must_be_max_filename_len;
    bool input_usb_keyboard;
    bool store_in_removable_device;
    char play_key;
    char record_key;
    const char* hello_music_file;
    const char* got_a_char_music_file;
    const char* start_play_music_file;
    const char* start_record_music_file;
    const char* reached_max_filename_len_music_file;
    const char* no_enough_filename_len_music_file;
    const char* click_music_file;
    const char* file_no_exit_music_file;
};

#define DEFAULT_SETTING {\
        10,              \
        "30",            \
        false,           \
        false,           \
        false,           \
        '*',             \
        '#',             \
        NULL,            \
        NULL,            \
        NULL,            \
        NULL,            \
        NULL,            \
        NULL,            \
        NULL,            \
        NULL,            \
    }

void usage(void)
{
    fprintf(stderr,
            "Usage: record-play [cmd] [argv]\n"
            "-l\t Set max length of record file name.(-l 8) default is 10.\n"
            "-L\t Set max length of record time in second.(-L 30) default is 30.\n"
            "-s\t Set this make every record file name must be max length. default is false\n"
            "-b\t Set this make input comes from USB keyboard. default is false\n"
            "-d\t Set this make all sound files saved in removable device(usb storage). default is false\n"
            "-P\t Set play key.(-P *) default is '*'\n"
            "-R\t Set record key.(-R *) default is '#'\n"
            "-H\t Set the music file played after program start.\n"
            "-C\t Set the music file played after got a char.\n"
            "-E\t Set the music file played before start play.\n"
            "-F\t Set the music file played before start record.\n"
            "-M\t Set the music file played after reached max filename length.\n"
            "-N\t Set the music file played after got play or record but filename length is too short.\n"
            "-O\t Set the music file played when can not find the file will be played.\n"
            "-K\t Set the music file played when record time is almost up.\n"
            "\n"
            "\n");
}

struct record_play rp = DEFAULT_SETTING;

static void play_music(const char* filename)
{
    char* cmd = NULL;
    if(NULL != filename) {
        if(access(filename, 0) != 0) {
            filename = rp.file_no_exit_music_file;
        }
        cmd = (char*)malloc(MAX_CMD_BUF_LEN*sizeof(*cmd));
        if(NULL == cmd) {
            return;
        }
        strcat(strcat(strcpy(cmd,"aplay "),filename)," &");
        system(cmd);
        free(cmd);
    }
}

static inline void rp_play_hello(void)
{
    play_music(rp.hello_music_file);
}

static inline void rp_play_start_play(void)
{
    play_music(rp.start_play_music_file);
}

static inline void rp_play_filename_len_too_short(void)
{
    play_music(rp.no_enough_filename_len_music_file);
}

static inline void rp_play_start_record(void)
{
    play_music(rp.start_record_music_file);
}

static inline void rp_play_got_a_key(void)
{
    play_music(rp.got_a_char_music_file);
}

static inline void rp_play_reached_max_filename_len(void)
{
    play_music(rp.reached_max_filename_len_music_file);
}

static inline void rp_play_click(void)
{
    play_music(rp.click_music_file);
}


static inline int get_low_row(void)
{
    int i = 0;
    for(i=0; i<__row_max; i++) {
        if(LOW == digitalRead(row[i])) {
            return i;
        }
    }
    return -1;
}

static inline void set_all_columns(int value)
{
    int i = 0;
    for(i=0; i<__column_max; i++) {
        digitalWrite(column[i], value);
    }
}

static char getch_matrix(void)
{
    int i = 0;
    int low_row = -1;

    /* set all columns to be low */
    set_all_columns(LOW);

    /* wait until rows not all high */
    while((low_row = get_low_row())<0) delay(10);

    /* have buttums down */
    for(i=0; i<__column_max; i++) {
        set_all_columns(HIGH);
        digitalWrite(column[i], LOW);
        if(get_low_row()<0) {
            continue;
        }
        else {
            return matrix_key[low_row][i];
        }
    }

    return 0x00;
}

static char rp_get_char(void)
{
    int ch = 0;

    if(rp.input_usb_keyboard) { /* read from usb keyboard */
        if(read(0, &ch, 1) < 0) {
            return 0x00;
        }
        if(ch > 0) {
            return (char)ch;
        }
        else {
            return 0x00;
        }
    }
    else { /* read from matrix keyboard */
        return getch_matrix();
    }
}

static bool file_name_len_check(const char* filename)
{
    if(rp.must_be_max_filename_len) {
        return (strlen(filename)==rp.max_filename_len);
    }
    else {
        int len = strlen(filename);
        return ((len>0) && (len<=rp.max_filename_len));
    }
}

static void rp_play(const char* filename)
{
    char* full_path_file = NULL;

    if(rp.store_in_removable_device) {
        full_path_file = (char*)malloc((strlen(REMOVABLE_DEVICE_SAVE_FOLDER)+
                                        strlen(filename)+sizeof(".wav"))*sizeof(*full_path_file));
        if(NULL == full_path_file) {
            return;
        }
        strcpy(full_path_file, REMOVABLE_DEVICE_SAVE_FOLDER);
        strcat(full_path_file, filename);
        strcat(full_path_file, ".wav");
    }
    else {
        full_path_file = (char*)malloc((strlen(IN_SYSTEM_SAVE_FOLDER)+
                                        strlen(filename)+sizeof(".wav"))*sizeof(*full_path_file));
        if(NULL == full_path_file) {
            return;
        }
        strcpy(full_path_file, IN_SYSTEM_SAVE_FOLDER);
        strcat(full_path_file, filename);
        strcat(full_path_file, ".wav");
    }
    play_music(full_path_file);
    free(full_path_file);
}

static void rp_record(const char* filename)
{
    char* cmd = NULL;
    if(NULL != filename) {
        cmd = (char*)malloc(MAX_CMD_BUF_LEN*sizeof(*cmd));
        if(NULL == cmd) {
            return;
        }
        if(rp.store_in_removable_device) {
            strcpy(cmd,"arecord -D \"plughw:1,0\" -d ");
            strcat(cmd,rp.max_record_time);
            strcat(cmd," ");
            strcat(cmd,REMOVABLE_DEVICE_SAVE_FOLDER);
            strcat(cmd, filename);
            strcat(cmd, ".wav &");
        }
        else {
            strcpy(cmd,"arecord -D \"plughw:1,0\" -d ");
            strcat(cmd,rp.max_record_time);
            strcat(cmd," ");
            strcat(cmd,IN_SYSTEM_SAVE_FOLDER);
            strcat(cmd, filename);
            strcat(cmd, ".wav &");
        }
        system(cmd);
        free(cmd);

        if(atoi(rp.max_record_time)>5) {
            int i = 0;
            sleep(atoi(rp.max_record_time)-5);
            for(i=0; i<5; i++) {
                rp_play_click();
                sleep(1);
            }
        }
    }
}

static void rp_loop(void)
{
    static char* filename = NULL;
    int filename_ptr = 0;
    char ch = 0x00;

    if(rp.max_filename_len<1) {
        fprintf(stderr,"max filename length %d is invalid\n", rp.max_filename_len);
        return;
    }

    filename = (char*)calloc(1, rp.max_filename_len+1);
    if(filename == NULL) {
        fprintf(stderr,"calloc filename failed\n");
        return;
    }

    while(true) {
        ch = rp_get_char();
        if(0x00 != ch) {
            if(rp.play_key == ch) {
                filename[filename_ptr] = '\0';
                if(file_name_len_check(filename)) {
                    rp_play_start_play();
                    rp_play(filename);
                }
                else {
                    rp_play_filename_len_too_short();
                }
                filename_ptr = 0;
                filename[filename_ptr] = '\0';
            }
            else if(rp.record_key == ch) {
                filename[filename_ptr] = '\0';
                if(file_name_len_check(filename)) {
                    rp_play_start_record();
                    rp_record(filename);
                }
                else {
                    rp_play_filename_len_too_short();
                }
                filename_ptr = 0;
                filename[filename_ptr] = '\0';
            }
            else {
                if(!isdigit(ch)) {
                    continue;
                }
                if(filename_ptr < rp.max_filename_len) {
                    rp_play_got_a_key();
                    filename[filename_ptr] = ch;
                    filename_ptr++;
                }
                else {
                    rp_play_reached_max_filename_len();
                    filename_ptr = 0;
                    filename[filename_ptr] = '\0';
                }
            }
        }
    }

    free(filename);
}

int main(int argc, char* argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "l:L:sbodP:R:")) != -1) {
        switch (ch) {
            case 'l':
                rp.max_filename_len = atoi(optarg);
                break;
            case 'L':
                rp.max_record_time = optarg;
                break;
            case 's':
                rp.must_be_max_filename_len = true;
                break;
            case 'b':
                rp.input_usb_keyboard = true;
                break;
            case 'd':
                rp.store_in_removable_device = true;
                break;
            case 'P':
                rp.play_key = *optarg;
                break;
            case 'R':
                rp.record_key = *optarg;
                break;
            case 'H':
                rp.hello_music_file = optarg;
                break;
            case 'C':
                rp.got_a_char_music_file = optarg;
                break;
            case 'E':
                rp.start_play_music_file = optarg;
                break;
            case 'F':
                rp.start_record_music_file = optarg;
                break;
            case 'M':
                rp.reached_max_filename_len_music_file = optarg;
                break;
            case 'N':
                rp.no_enough_filename_len_music_file = optarg;
                break;
            case 'K':
                rp.click_music_file = optarg;
                break;
            default:
                usage();
                return -1;
        }
    }

    if(wiringPiSetup() < 0) {
        return -1;
    }
    if(!rp.input_usb_keyboard) {
        int i = 0;
        for(i=0; i<__row_max; i++) {
            pinMode(row[i], INPUT);
            pullUpDnControl(row[i], PUD_UP);
        }
        for(i=0; i<__column_max; i++) {
            pinMode(column[i], OUTPUT);
        }
    }

    rp_play_hello();

    rp_loop();

    return -1;
}

