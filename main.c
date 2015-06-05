#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wiringPi.h>

#define IN_SYSTEM_SAVE_FOLDER            "/home/pi/records/onboardSD/"
#define REMOVABLE_DEVICE_SAVE_FOLDER     "/home/pi/records/usb/"

#define MAX_CMD_BUF_LEN                  (1024)
//#define DBG_PRINT         fprintf
#define DBG_PRINT(...)

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
#define row_mask     (0x8D)

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
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','R'},
    {'*','0','#','P'},
};

struct record_play {
    int filename_len;
    const char* max_record_time;
    bool input_usb_keyboard;
    bool store_in_removable_device;
    char play_key;
    char record_key;
    const char* pickup_music_file;
    const char* input_play_keys_music_file;
    const char* got_a_char_music_file;
    const char* start_play_music_file;
    const char* finish_play_music_file;
    const char* input_record_keys_music_file;
    const char* start_record_music_file;
    const char* finish_record_music_file;
    const char* finish_record_wait_drop_down_music_file;
    const char* reached_max_filename_len_music_file;
    const char* no_enough_filename_len_music_file;
    const char* file_no_exit_music_file;
    const char* click_music_file;
};

#define DEFAULT_SETTING {\
        7,               \
        "30",            \
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
            "-l\t Set length of record file name.(-l 8) default is 7.\n"
            "-L\t Set max length of record time in second.(-L \"30\") default is \"30\".\n"
            "-b\t Set this make input comes from USB keyboard. default is false\n"
            "-d\t Set this make all sound files saved in removable device(usb storage). default is false\n"
            "-P\t Set play key.(-P *) default is '*'\n"
            "-R\t Set record key.(-R *) default is '#'\n"
            "-H\t Set the music file played after pick up.\n"
            "-I\t Set the music file played before input play keys.\n"
            "-C\t Set the music file played after got a char.\n"
            "-E\t Set the music file played before start play.\n"
            "-J\t Set the music file played after finished play.\n"
            "-Q\t Set the music file played before input record keys.\n"
            "-F\t Set the music file played before start record.\n"
            "-T\t Set the music file played after finished record.\n"
            "-S\t Set the music file played after finished record and wait drop down phone.\n"
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
            if(filename == NULL) {
                return;
            }
        }
        cmd = (char*)malloc(MAX_CMD_BUF_LEN*sizeof(*cmd));
        if(NULL == cmd) {
            return;
        }
        strcpy(cmd,"aplay ");
        strcat(cmd,filename);
        //strcat(cmd," &");
        printf("Start play. Cmd=%s", cmd);
        system(cmd);
        free(cmd);
    }
}

static inline void rp_play_pick_up(void)
{
    play_music(rp.pickup_music_file);
}

static inline void rp_play_input_play_keys(void)
{
    play_music(rp.input_play_keys_music_file);
}

static inline void rp_play_start_play(void)
{
    play_music(rp.start_play_music_file);
}

static inline void rp_play_finish_play(void)
{
    play_music(rp.finish_play_music_file);
}

static inline void rp_play_input_record_keys(void)
{
    play_music(rp.input_record_keys_music_file);
}

static inline void rp_play_filename_len_too_short(void)
{
    play_music(rp.no_enough_filename_len_music_file);
}

static inline void rp_play_start_record(void)
{
    play_music(rp.start_record_music_file);
}

static inline void rp_play_finish_record(void)
{
    play_music(rp.finish_record_music_file);
}

static inline void rp_play_finish_record_wait_dorp(void)
{
    play_music(rp.finish_record_wait_drop_down_music_file);
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
    while((low_row = get_low_row())<0) delay(50);

    /* have buttums down */
    for(i=0; i<__column_max; i++) {
        set_all_columns(HIGH);
        digitalWrite(column[i], LOW);
        if(get_low_row()<0) {
            continue;
        }
        else {
            while(get_low_row() >= 0);
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
    return (strlen(filename)==rp.filename_len);
}

static void rp_play(const char* filename)
{
    char* full_path_file = NULL;

    if(rp.store_in_removable_device) {
        full_path_file = (char*)malloc(MAX_CMD_BUF_LEN*sizeof(*full_path_file));
        if(NULL == full_path_file) {
            return;
        }
        strcpy(full_path_file, REMOVABLE_DEVICE_SAVE_FOLDER);
        strcat(full_path_file, filename);
        strcat(full_path_file, ".wav");
    }
    else {
        full_path_file = (char*)malloc(MAX_CMD_BUF_LEN*sizeof(*full_path_file));
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
        printf("Start record. Cmd=%s", cmd);
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

static void rp_get_filename(char* filename)
{
    int i = 0;
    char ch;
    while(i < rp.filename_len) {
        ch = rp_get_char();
        if(0x00 != ch) {
            DBG_PRINT(stderr,"got ch=%c\n", ch);
            if(!isdigit(ch)) {
                continue;
            }
            rp_play_got_a_key();
            filename[i] = ch;
            i++;
        }
    }
    filename[i] = '\0';
}

static void rp_loop(void)
{
    static char* filename = NULL;
    char ch = 0x00;

    if(rp.filename_len<1) {
        fprintf(stderr,"max filename length %d is invalid\n", rp.filename_len);
        return;
    }

    filename = (char*)calloc(1, rp.filename_len+1);
    if(filename == NULL) {
        fprintf(stderr,"calloc filename failed\n");
        return;
    }

    while(true) {
        ch = rp_get_char();
        if('P' == ch) { /* pick up phone */
            do {
                rp_play_pick_up();
                ch = rp_get_char();
            }
            while(('*'!=ch) && ('#'!=ch));
            if('*' == ch) { /* start play sounds */
                rp_play_input_play_keys();
                rp_get_filename(filename);
                rp_play_start_play();
                rp_play(filename);
                do {
                    rp_play_finish_play();
                    ch = rp_get_char();
                }
                while(('P'!=ch) && ('R'!=ch));
                if('P' == ch) { /* drop down phone */
                    ;
                }
                if('R' == ch) { /* start record */
                    ch = '#';
                }
            }
            if('#' == ch) { /* start record sounds */
                rp_play_input_record_keys();
                rp_get_filename(filename);
RERECORD:
                rp_play_start_record();
                rp_record(filename);
RECOMFIRM:
                do {
                    rp_play_finish_record();
                    ch = rp_get_char();
                }
                while(('0'!=ch) && ('*'!=ch) && ('#'!=ch));
                if('0' == ch) { /* rerecord */
                    goto RERECORD;
                }
                if('*' == ch) { /* try to play record sound */
                    rp_play(filename);
                    goto RECOMFIRM;
                }
                if('#' == ch) { /* confirm */
                    do {
                        rp_play_finish_record_wait_dorp();
                        ch = rp_get_char();
                    }
                    while('P'!=ch);
                }
            }
        }
    }

    free(filename);
}

int main(int argc, char* argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "l:L:bdP:R:H:I:C:E:J:Q:F:T:S:M:N:K:O:")) != -1) {
        switch (ch) {
            case 'l':
                rp.filename_len = atoi(optarg);
                break;
            case 'L':
                rp.max_record_time = optarg;
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
                rp.pickup_music_file = optarg;
                break;
            case 'I':
                rp.input_play_keys_music_file = optarg;
                break;
            case 'C':
                rp.got_a_char_music_file = optarg;
                break;
            case 'E':
                rp.start_play_music_file = optarg;
                break;
            case 'J':
                rp.finish_play_music_file = optarg;
                break;
            case 'Q':
                rp.input_record_keys_music_file = optarg;
                break;
            case 'F':
                rp.start_record_music_file = optarg;
                break;
            case 'T':
                rp.finish_record_music_file = optarg;
                break;
            case 'S':
                rp.finish_record_wait_drop_down_music_file = optarg;
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
            case 'O':
                rp.file_no_exit_music_file = optarg;
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

    rp_loop();

    return -1;
}

