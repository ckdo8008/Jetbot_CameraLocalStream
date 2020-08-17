#include <signal.h>
#include <gst/gst.h>
#include <stdio.h>

//int main(int argc, char* argv[])
//{
//    GstElement* pipeline;
//    GstBus* bus;
//    GstMessage* msg;
//
//    /* Initialize GStreamer */
//    gst_init(&argc, &argv);
//
//    /* Build the pipeline */
//    pipeline =
//        gst_parse_launch
//        (
//            "nvarguscamerasrc ! "
//            "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
//            "nvv4l2vp8enc bitrate=4000000 ! rtpvp8pay mtu=1400 ! decodebin name=t ! "
//            "queue ! udpsink host=127.0.0.1 port=5001 sync=false async=false"
//            "t. ! queue ! udpsink host=127.0.0.1 port=5002 sync=false async=false",
//            NULL);
//
//    /* Start playing */
//    gst_element_set_state(pipeline, GST_STATE_PLAYING);
//
//    /* Wait until error or EOS */
//    bus = gst_element_get_bus(pipeline);
//    msg =
//        gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
//            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
//    /* Free resources */
//    if (msg != NULL)
//        gst_message_unref(msg);
//    gst_object_unref(bus);
//    gst_element_set_state(pipeline, GST_STATE_NULL);
//    gst_object_unref(pipeline);
//    return 0;
//}
GMainLoop* loop;
GstBus* bus;
GstMessage* msg;
GstElement* pipeline, * source, * filter, * codec, * wrapper5001, * wrapper5002;
GstElement* tee, * queue5001, * queue5002;

void mystop(int signo)
{
    //gst_element_set_state(pipeline, GST_STATE_NULL);
    g_main_loop_quit(loop);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_main_loop_unref(loop);

    gst_object_unref(filter);
    gst_object_unref(codec);
    gst_object_unref(wrapper5001);
    gst_object_unref(wrapper5002);

    gst_object_unref(queue5001);
    gst_object_unref(queue5002);
    gst_object_unref(tee);

    gst_object_unref(bus);
    gst_object_unref(pipeline);

    if (msg != NULL)
        gst_message_unref(msg);
}

gboolean message_cb(GstBus* bus, GstMessage* message, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError* err = NULL;
        gchar* name, * debug = NULL;

        name = gst_object_get_path_string(message->src);
        gst_message_parse_error(message, &err, &debug);

        g_printerr("ERROR: from element %s: %s\n", name, err->message);
        if (debug != NULL)
            g_printerr("Additional debug info:\n%s\n", debug);

        g_error_free(err);
        g_free(debug);
        g_free(name);

        g_main_loop_quit(loop);
        break;
    }
    case GST_MESSAGE_WARNING: {
        GError* err = NULL;
        gchar* name, * debug = NULL;

        name = gst_object_get_path_string(message->src);
        gst_message_parse_warning(message, &err, &debug);

        g_printerr("ERROR: from element %s: %s\n", name, err->message);
        if (debug != NULL)
            g_printerr("Additional debug info:\n%s\n", debug);

        g_error_free(err);
        g_free(debug);
        g_free(name);
        break;
    }
    case GST_MESSAGE_EOS: {
        g_print("Got EOS\n");

        //g_main_loop_quit(loop);
        //gst_element_set_state(pipeline, GST_STATE_NULL);
        //g_main_loop_unref(loop);
        //gst_object_unref(pipeline);
        mystop(0);
        exit(0);
        break;
    }
    default:
        break;
    }

    return TRUE;
}

int main(int argc, char* argv[]) {
    //GstElement* pipeline, * source, * filter;
    GstElement* sinkudp5001, * sinkudp5002;
    //GstElement*tee, * tee00_queue, * sinkudp5001, * sinkudp5002;
    GstCaps* videosrc_caps;


    signal(SIGINT, mystop);
    signal(SIGQUIT, mystop);
    signal(SIGKILL, mystop);
    signal(SIGTERM, mystop);
    gst_init(&argc, &argv);

    pipeline = gst_pipeline_new("test-pipeline");
    source = gst_element_factory_make("nvarguscamerasrc", "source");
    filter = gst_element_factory_make("capsfilter", "filter");

    codec = gst_element_factory_make("nvv4l2vp8enc", "codec");

    wrapper5001 = gst_element_factory_make("rtpvp8pay", "wrapper5001");
    wrapper5002 = gst_element_factory_make("rtpvp8pay", "wrapper5002");
    sinkudp5001 = gst_element_factory_make("udpsink", "sinkudp5001");
    sinkudp5002 = gst_element_factory_make("udpsink", "sinkudp5002");

    tee = gst_element_factory_make("tee", "tee");

    queue5001 = gst_element_factory_make("queue", "queue5001");
    queue5002 = gst_element_factory_make("queue", "queue5002");

    //sinkudp5001 = gst_element_factory_make("udpsink", "sinkudp5001");
    //sinkudp5002 = gst_element_factory_make("udpsink", "sinkudp5002");

    if (!pipeline || !source || !filter || !codec || !wrapper5001 || !wrapper5002 || !sinkudp5001 || !sinkudp5002 || !tee || !queue5001 || !queue5002) {
        g_error("Failed to create elements");
        return -1;
    }

    gst_bin_add_many(GST_BIN(pipeline), source, filter, codec, tee, queue5001, wrapper5001, sinkudp5001, queue5002, wrapper5002, sinkudp5002, NULL);
    if (!gst_element_link_many(source, filter, codec, tee, NULL) ||
        !gst_element_link_many(tee, queue5001, wrapper5001, sinkudp5001, NULL) ||
        !gst_element_link_many(tee, queue5002, wrapper5002, sinkudp5002, NULL)) {
        g_error("Failed to link elements");
        return -2;
    }

    videosrc_caps = gst_caps_new_simple("video/x-raw(memory:NVMM)",
        "width", G_TYPE_INT, 1920,
        "height", G_TYPE_INT, 1080,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    g_object_set(G_OBJECT(filter), "caps", videosrc_caps, NULL);
    //g_object_set(G_OBJECT(codec), "bitrate", 4000000, NULL);
    g_object_set(G_OBJECT(wrapper5001), "mtu", 1400, NULL);
    g_object_set(G_OBJECT(wrapper5002), "mtu", 1400, NULL);

    g_object_set(G_OBJECT(sinkudp5001), "host", "127.0.0.1", "port", 5001, NULL);
    g_object_set(G_OBJECT(sinkudp5002), "host", "127.0.0.1", "port", 5002, NULL);
    gst_caps_unref(videosrc_caps);

    //gst_element_set_state(pipeline, GST_STATE_PLAYING);

    //bus = gst_element_get_bus(pipeline);
    //msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    loop = g_main_loop_new(NULL, FALSE);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(message_cb), NULL);
    gst_object_unref(GST_OBJECT(bus));

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    //mystop(0);
    g_print("Starting loop");
    g_main_loop_run(loop);
    mystop(0);
    return 0;
}