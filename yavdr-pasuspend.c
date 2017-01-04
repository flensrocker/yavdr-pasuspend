/***
  yavdr-pasuspend is based on pasuspender, which is part of PulseAudio.
  Copyright 2017 Lars Hanisch <dvb@flensrocker.de>

  pasuspender:
  Copyright 2004-2006 Lennart Poettering

  yavdr-pasuspend is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  yavdr-pasuspend is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with yavdr-pasuspend; if not, see <http://www.gnu.org/licenses/>.
***/

#include <stdio.h>
#include <getopt.h>
#include <locale.h>

#include <pulse/pulseaudio.h>

static int suspend = 1;
static pa_context *context = NULL;
static pa_mainloop_api *mainloop_api = NULL;

static void quit(int ret) {
    mainloop_api->quit(mainloop_api, ret);
}

static void context_drain_complete(pa_context *c, void *userdata) {
    pa_context_disconnect(c);
}

static void drain(void) {
    pa_operation *o;

    if (context) {
        if (!(o = pa_context_drain(context, context_drain_complete, NULL)))
            pa_context_disconnect(context);
        else
            pa_operation_unref(o);
    } else
        quit(0);
}

static void suspend_complete(pa_context *c, int success, void *userdata) {
    static int n = 0;

    n++;

    if (!success) {
        fprintf(stderr, "Failure to suspend: %s\n", pa_strerror(pa_context_errno(c)));
        quit(1);
        return;
    }

    if (n >= 2)
        drain(); /* drain and quit */
}

static void context_state_callback(pa_context *c, void *userdata) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            if (pa_context_is_local(c)) {
                pa_operation_unref(pa_context_suspend_sink_by_index(c, PA_INVALID_INDEX, suspend, suspend_complete, NULL));
                pa_operation_unref(pa_context_suspend_source_by_index(c, PA_INVALID_INDEX, suspend, suspend_complete, NULL));
            } else {
                fprintf(stderr, "WARNING: Sound server is not local, not suspending.\n");
            }

            break;

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            fprintf(stderr, "Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
            pa_context_unref(context);
            context = NULL;
            quit(1);
            break;
    }
}

static void help(const char *argv0) {

    printf("%s [options]\n\n"
           "  -h, --help                            Show this help\n"
           "  -s, --suspend                         Suspend PulseAudio\n"
           "  -r, --resume                          Resume PulseAudio\n\n",
           argv0);
}

int main(int argc, char *argv[]) {
    pa_mainloop* m = NULL;
    int c, ret = 1;
    char *bn;

    static const struct option long_options[] = {
        {"suspend",     0, NULL, 's'},
        {"resume",      0, NULL, 'r'},
        {"help",        0, NULL, 'h'},
        {NULL,          0, NULL, 0}
    };

    setlocale(LC_ALL, "");

    bn = pa_path_get_filename(argv[0]);

    while ((c = getopt_long(argc, argv, "rsh", long_options, NULL)) != -1) {
        switch (c) {
            case 'h' :
                help(bn);
                ret = 0;
                goto quit;

            case 'r':
                suspend = 0;
                break;

            case 's':
                suspend = 1;
                break;

            default:
                goto quit;
        }
    }

    if (!(m = pa_mainloop_new())) {
        fprintf(stderr, "pa_mainloop_new() failed.\n");
        goto quit;
    }

    mainloop_api = pa_mainloop_get_api(m);
    pa_signal_init(mainloop_api);

    if (!(context = pa_context_new(mainloop_api, bn))) {
        fprintf(stderr, "pa_context_new() failed.\n");
        goto quit;
    }

    pa_context_set_state_callback(context, context_state_callback, NULL);

    if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(context)));
        goto quit;
    }

    if (pa_mainloop_run(m, &ret) < 0) {
        fprintf(stderr, "pa_mainloop_run() failed.\n");
        goto quit;
    }

quit:
    if (context)
        pa_context_unref(context);

    if (m) {
        pa_signal_done();
        pa_mainloop_free(m);
    }

    return ret;
}
