#define LF_RUNARA
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdbool.h>
#include <memory.h>
#include <leif/win.h>
#include <leif/ui_core.h>
#include <leif/leif.h>

static int32_t masterfd;

typedef struct {
    uint32_t codepoint;
} Cell;

int32_t utf8decode(const char *s, uint32_t *out_cp) {
    unsigned char c = s[0];
    if (c < 0x80) {
        *out_cp = c;
        return 1;
    } else if ((c >> 5) == 0x6) {
        *out_cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    } else if ((c >> 4) == 0xE) {
        *out_cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    } else if ((c >> 3) == 0x1E) {
        *out_cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return -1; // invalid UTF-8
}

void _lf_ui_core_next_event(lf_ui_state_t* ui) {
    float cur_time = lf_ui_core_get_elapsed_time();
    ui->delta_time = cur_time - ui->_last_time;
    ui->_last_time = cur_time;

    bool rendered = lf_windowing_get_current_event() == LF_EVENT_WINDOW_REFRESH;

    lf_ui_core_shape_widgets_if_needed(ui, ui->root, false);

    if (ui->needs_render) {
        lf_win_make_gl_context(ui->win);
        vec2s winsize = lf_win_get_size(ui->win);
        ui->render_clear_color_area(
            lf_color_from_hex(0x1a1a1a), LF_SCALE_CONTAINER(winsize.x, winsize.y), winsize.y);
        ui->render_begin(ui->render_state);
        ui->render_rect(ui->render_state, (vec2s){50, 50}, (vec2s){50,50}, LF_RED,
            LF_NO_COLOR, 0.0f, 0.0f);
        ui->render_end(ui->render_state);
        lf_win_swap_buffers(ui->win);
        ui->needs_render = false;
        rendered = true;
    }
}

size_t readfrompty(void) {
    static char buf[__SHRT_MAX__];
    static uint32_t buflen = 0;

    int32_t nbytes = read(masterfd, buf + buflen, sizeof(buf) - buflen);
    buflen += nbytes;

    uint32_t iter = 0;
    while (iter < buflen) {
        uint32_t codepoint;
        int32_t len = utf8decode(&buf[iter], &codepoint);
        if (len == -1 || len > buflen) break;
        printf("%i\n", codepoint);
        iter += len;
    }

    if (iter < buflen) {
        memmove(buf, buf + iter, buflen - iter);
    }

    buflen -= iter;

    return nbytes;
}

int main(void) {
    if(forkpty(&masterfd, NULL, NULL, NULL) == 0) {
        execlp("/usr/bin/bash", "bash", NULL);
        perror("execlp");
        exit(1);
    }

    lf_windowing_init();

    lf_window_t win = lf_ui_core_create_window(800, 600, "Chris Terminal Emulator");
    lf_ui_state_t* ui = lf_ui_core_init(win);

    fd_set fdset;
    int32_t glfwfd = 0;
    //glfwMakeContextCurrent(win);

    bool first_time = true;
    
    while (ui->running) {
        FD_ZERO(&fdset);
        FD_SET(masterfd, &fdset);
        //FD_SET(glfwfd, &fdset);

        if (first_time) {
            _lf_ui_core_next_event(ui);
            first_time = false;
        }

        select(masterfd+1, &fdset, NULL, NULL, NULL);

        if (FD_ISSET(masterfd, &fdset)) {
           readfrompty();
        }

        if (FD_ISSET(masterfd, &fdset)) {
            lf_windowing_next_event();

            lf_event_type_t curevent = lf_windowing_get_current_event();
            if (curevent == LF_EVENT_KEY_PRESS
                || curevent == LF_EVENT_TYPING_CHAR
                || curevent == LF_EVENT_WINDOW_CLOSE
                || curevent == LF_EVENT_WINDOW_REFRESH
                || curevent == LF_EVENT_WINDOW_RESIZE) {
                    _lf_ui_core_next_event(ui);
                    printf("Called.\n");
                }
        }
    }

    printf("Hello, World!\n");
    return EXIT_SUCCESS;
}