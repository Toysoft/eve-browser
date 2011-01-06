#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

#include <linux/input.h>

#include "js_extension.h"
#include "css_extension.h"

static GtkWidget* main_window;
static GtkWidget* uri_entry;
static GtkStatusbar* main_statusbar;
static GtkWidget* toolbar;
static GtkToolItem* itemUrl;
static GtkScrolledWindow* scrolled_window;
static WebKitWebView* web_view;
static gchar* main_title;
static gdouble load_progress;
static guint status_context_id;
static GtkWidget* window ;
static GtkWidget* vbox;

unsigned int g_framebuffer_width = 1280;
unsigned int g_framebuffer_height = 720;
float g_default_scale = 1.0f;
///////////////////////////
///////////////////////////

static void window_object_cleared_cb(   WebKitWebView *frame,
                                        gpointer context,
                                        gpointer arg3,
                                        gpointer user_data)
{
    printf("window_object_cleared_cb\n");

    registerJsFunctions(web_view);
}

///////////////////////7

static void
activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
    const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
    g_assert (uri);
    webkit_web_view_load_uri (web_view, uri);
}

static void
update_title (GtkWindow* window)
{
    GString* string = g_string_new (main_title);
    g_string_append (string, " - WebKit Launcher");
    if (load_progress < 100)
        g_string_append_printf (string, " (%f%%)", load_progress);
    gchar* title = g_string_free (string, FALSE);
    gtk_window_set_title (window, title);
    g_free (title);
}

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data)
{
    /* underflow is allowed */
    gtk_statusbar_pop (main_statusbar, status_context_id);
    if (link)
        gtk_statusbar_push (main_statusbar, status_context_id, link);
}

static void
notify_title_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
    if (main_title)
        g_free (main_title);
    main_title = g_strdup (webkit_web_view_get_title(web_view));
    update_title (GTK_WINDOW (main_window));
}

static void
notify_load_status_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
    if (webkit_web_view_get_load_status (web_view) == WEBKIT_LOAD_COMMITTED) {
        WebKitWebFrame* frame = webkit_web_view_get_main_frame (web_view);
        const gchar* uri = webkit_web_frame_get_uri (frame);
        if (uri)
            gtk_entry_set_text (GTK_ENTRY (uri_entry), uri);
    }
}

static void
notify_progress_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
    load_progress = webkit_web_view_get_progress (web_view) * 100;
    update_title (GTK_WINDOW (main_window));
}

static void
destroy_cb (GtkWidget* widget, gpointer data)
{
    gtk_main_quit ();
}


static gboolean
focus_out_cb (GtkWidget* widget, GdkEvent * event, gpointer data)
{
    printf("%s > \n", __func__);
    gtk_widget_grab_focus(widget);
    return false;
}


static void
document_load_finished_cb (GtkWidget* widget, WebKitWebFrame * arg1, gpointer data)
{
    registerCssExtension(web_view);

    // Only use if debugging is needed
    //registerSpecialJsFunctions(web_view);
}


void goBack()
{
    webkit_web_view_go_back(web_view);
}

void gtk_widget_set_can_focus(GtkWidget* wid, gboolean can)
{
    if(can)
        GTK_WIDGET_SET_FLAGS(wid, GTK_CAN_FOCUS);
    else
        GTK_WIDGET_UNSET_FLAGS(wid, GTK_CAN_FOCUS);
}

static GtkScrolledWindow*
create_browser ()
{
    scrolled_window = (GtkScrolledWindow*)gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    webkit_web_view_set_transparent(web_view, true);
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));
    webkit_web_view_set_full_content_zoom(web_view, true);

    g_signal_connect (web_view, "notify::title", G_CALLBACK (notify_title_cb), web_view);
    g_signal_connect (web_view, "notify::load-status", G_CALLBACK (notify_load_status_cb), web_view);
    g_signal_connect (web_view, "notify::progress", G_CALLBACK (notify_progress_cb), web_view);
    g_signal_connect (web_view, "hovering-over-link", G_CALLBACK (link_hover_cb), web_view);

    g_signal_connect (web_view, "focus-out-event", G_CALLBACK (focus_out_cb), web_view);

    g_signal_connect (web_view, "document-load-finished", G_CALLBACK (document_load_finished_cb), web_view);

    g_signal_connect (web_view, "window_object_cleared", G_CALLBACK (window_object_cleared_cb), web_view);

    return scrolled_window;
}

static GtkWidget*
create_statusbar ()
{
    main_statusbar = GTK_STATUSBAR (gtk_statusbar_new ());
    gtk_widget_set_can_focus(GTK_WIDGET (main_statusbar), false);
    status_context_id = gtk_statusbar_get_context_id (main_statusbar, "Link Hover");
    
    return (GtkWidget*)main_statusbar;
}

static GtkWidget*
create_toolbar ()
{
    toolbar = gtk_toolbar_new ();
    gtk_widget_set_can_focus(GTK_WIDGET (toolbar), false);

#if GTK_CHECK_VERSION(2,15,0)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
    gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);


    /* The URL entry */
    itemUrl = gtk_tool_item_new ();
    gtk_widget_set_can_focus(GTK_WIDGET (itemUrl), false);
    gtk_tool_item_set_expand (itemUrl, TRUE);
    uri_entry = gtk_entry_new ();
    gtk_container_add (GTK_CONTAINER (itemUrl), uri_entry);
    g_signal_connect (G_OBJECT (uri_entry), "activate", G_CALLBACK (activate_uri_entry_cb), NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), itemUrl, -1);

    return toolbar;
}

/**
 * This toogles the background of the window
 **/
static void
toogleBackground (void)
{
    printf("%s > \n", __func__);
    static gboolean isTransparent = false;
    isTransparent = !isTransparent;
    webkit_web_view_set_transparent(web_view, isTransparent);
}

static gboolean isShown = true;

static void
toogleMode (void)
{
    printf("%s > \n", __func__);
    if(isShown)
    {
        gtk_widget_set_size_request(vbox, g_framebuffer_width, g_framebuffer_height);
        gtk_widget_hide(GTK_WIDGET (main_statusbar));
        gtk_widget_hide(GTK_WIDGET (toolbar));
    }
    else
    {
        gtk_widget_set_size_request(vbox, g_framebuffer_width-200, g_framebuffer_height);
        gtk_widget_show(GTK_WIDGET (main_statusbar));
        gtk_widget_show(GTK_WIDGET (toolbar));

        gtk_widget_grab_focus(GTK_WIDGET (itemUrl));
    }
    isShown = !isShown;
}

static gboolean gIsNumLock = false;
static void toogleNumLock()
{
    gIsNumLock = !gIsNumLock;
}

static gboolean gIsZoomLock = false;
static void toggleZoomLock()
{
    gIsZoomLock = !gIsZoomLock;
}

static void handleZoomLock(int value)
{
    if(value > 0)
        webkit_web_view_zoom_in(web_view);
    else if(value < 0)
        webkit_web_view_zoom_out(web_view);
    else
        webkit_web_view_set_zoom_level(web_view, g_default_scale );
}

static gboolean
on_key_press (GtkWidget* widget, GdkEventKey *event, gpointer data)
{
    if (event->type == GDK_KEY_PRESS)
    {
        /*printf("event +++\n");
        printf("event->send_event: %02X\n", event->send_event);
        printf("event->state: %02X\n", event->state);
        printf("event->keyval: %02X\n", event->keyval);
        printf("event->length: %02X\n", event->length);
        printf("event->string: %s\n",  event->string);
        printf("event->hardware_keycode: %02X\n", event->hardware_keycode);
        printf("event->group: %02X\n", event->group);
        printf("event->is_modifier: %02X\n", event->is_modifier);
        printf("event ---\n");*/

        /* GDK has a bug regarding keycode capturing with greater codes than 128. this clause corrects this error */
        /* At the moment only KEY_OK to GDK_Return mapping is checked (proof of concept) */
        if(event->keyval == 0xFFFFFF)
        {
            printf("%s:%s[%d] corrupt keyval (%02X)\n", __FILE__, __func__, __LINE__, event->hardware_keycode);
            gboolean rtv;
            int corrected = 0;
            switch(event->hardware_keycode)
            {
                case KEY_OK:
                    event->keyval = GDK_Return;
                    event->state = 0;
                    corrected = 1;
                    break;
                case KEY_RED:
                    event->keyval = GDK_F5;
                    event->state = 0;
                    corrected = 1;
                    break;
                case KEY_GREEN:
                    event->keyval = GDK_F5;
                    event->state = 0;
                    corrected = 1;
                    break;
                case KEY_YELLOW:
                    event->keyval = GDK_F7;
                    event->state = 0;
                    corrected = 1;
                    break;
                case KEY_BLUE:
                    event->keyval = GDK_F8;
                    event->state = 0;
                    corrected = 1;
                    break;
                default:
                    corrected = 0;
                    break;
            }
                   
            if(corrected) {
                printf("%s:%s[%d] corrected\n", __FILE__, __func__, __LINE__);
                gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            }
            return true;
        }


        if(gIsZoomLock)
        {
            switch(event->keyval)
            {
            case GDK_Up:     handleZoomLock(+1); break;
            case GDK_Return: handleZoomLock(0); break;
            case GDK_Down:   handleZoomLock(-1); break;
            default: break;
            }
        }

        if (event->keyval == GDK_F1)
        {
            toogleMode();
        }
        else if (isShown && event->keyval == GDK_F2)
        {
            //toogleBackground();
            toggleZoomLock();
        }
        else if (isShown && event->keyval == GDK_Num_Lock)
        {
            toogleNumLock();
        }
        else if (isShown && event->keyval == GDK_F4)
        {
            gboolean rtv;
            event->keyval = GDK_Tab;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_F3)
        {
            gboolean rtv;
            event->keyval = GDK_Tab;
            event->state |= GDK_SHIFT_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval >= 0xFFB0 && event->keyval <= 0xFFB9)
        {
            gIsNumLock = true;
            //event->state |= GDK_MOD1_MASK;
            switch(event->keyval) {
            case GDK_KP_1: event->keyval = '.'; break;
            case GDK_KP_2: event->keyval = 'a'; break;
            case GDK_KP_3: event->keyval = 'd'; break;
            case GDK_KP_4: event->keyval = 'g'; break;
            case GDK_KP_5: event->keyval = 'j'; break;
            case GDK_KP_6: event->keyval = 'm'; break;
            case GDK_KP_7: event->keyval = 'p'; break;
            case GDK_KP_8: event->keyval = 't'; break;
            case GDK_KP_9: event->keyval = 'w'; break;
            case GDK_KP_0: event->keyval = '+'; break;
            default: break;
            }
        }
        else if (isShown && event->keyval == GDK_KP_Down )
        {
            gboolean rtv;
            event->keyval = '2';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Up )
        {
            gboolean rtv;
            event->keyval = '8';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Left )
        {
            gboolean rtv;
            event->keyval = '4';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Right )
        {
            gboolean rtv;
            event->keyval = '6';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Home )
        {
            gboolean rtv;
            event->keyval = '7';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Page_Up )
        {
            gboolean rtv;
            event->keyval = '9';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_End )
        {
            gboolean rtv;
            event->keyval = '1';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Page_Down )
        {
            gboolean rtv;
            event->keyval = '3';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Insert || event->keyval == 0xFFFFFF)
        {
            gboolean rtv;
            event->keyval = '0';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (isShown && event->keyval == GDK_KP_Begin )
        {
            gboolean rtv;
            event->keyval = '5';
            event->state |= GDK_MOD1_MASK;
            gtk_signal_emit_by_name(GTK_OBJECT (window), "key-press-event", event, &rtv);
            return true;
        }
        else if (event->keyval == GDK_BackSpace)
            goBack();
    }
    return false;
}


static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event)
{
    printf("%s\n", __func__);
    cairo_t *cr;

    cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return false;
}

static GtkWidget*
create_window ()
{
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), g_framebuffer_width, g_framebuffer_height);
    gtk_widget_set_name (window, "eve-browser");
    gtk_window_set_decorated(GTK_WINDOW(window), false);

    GdkScreen *screen = gtk_widget_get_screen(window);
    gtk_widget_set_colormap(window, gdk_screen_get_rgba_colormap(screen));
    gtk_widget_set_app_paintable(window, true);
    gtk_widget_realize(window);
    gdk_window_set_back_pixmap(window->window, NULL, false);

    g_signal_connect(window, "expose-event", G_CALLBACK(expose_event), window);

    g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), NULL);
    g_signal_connect (window, "key-press-event", G_CALLBACK (on_key_press), NULL);

    //g_signal_connect (window, "expose-event", G_CALLBACK (expose_event), NULL);
    //g_signal_connect (window, "screen-changed", G_CALLBACK (screen_changed), NULL);
    return window;
}


int
main (int argc, char* argv[])
{
    printf("%s:%s[%d]\n", __FILE__, __func__, __LINE__);
    unsigned char haveUrl = 0;
    int argCount = argc - 1;

    printf("%s:%s[%d] argCount=%d\n", __FILE__, __func__, __LINE__, argCount);

    for(int i = 0; i < argCount; i++)
    {
        if     (!strncmp("-s", argv[1 + i], 2))
        {
            printf("%s:%s[%d] parameter scale %s\n", __FILE__, __func__, __LINE__, argv[1 + i]);
            
            sscanf(argv[1 + i], "%dx%d", &g_framebuffer_width, &g_framebuffer_height);
            i++;
            printf("%s:%s[%d] fw=%d fh=%d\n", __FILE__, __func__, __LINE__, g_framebuffer_width, g_framebuffer_height);
        }
        else
            haveUrl = 1; 
    }

    gtk_init (&argc, &argv);
    if (!g_thread_supported ())
        g_thread_init (NULL);


    GtkWidget* fixed = gtk_fixed_new();
    //screen_changed(fixed, NULL, NULL);
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_toolbar (), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (create_browser ()), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_statusbar (), FALSE, FALSE, 0);

    main_window = create_window ();

    gtk_fixed_put(GTK_FIXED(fixed), vbox, 0, 0);
    gtk_widget_set_size_request(vbox, g_framebuffer_width, g_framebuffer_height);

    GtkWidget* statusLabel = gtk_label_new ("Status");
    gtk_fixed_put(GTK_FIXED(fixed), statusLabel, g_framebuffer_width - 200, 0);
    gtk_widget_set_size_request(statusLabel, 200, 100);

    gtk_container_add (GTK_CONTAINER (main_window), fixed);

    gchar* uri = (gchar*) (haveUrl ? argv[argCount] : "http://www.google.com/");
    webkit_web_view_load_uri (web_view, uri);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (main_window);

    toogleMode();

    g_default_scale = g_framebuffer_width / 1280.0f;
    handleZoomLock(0);

    gtk_main ();

    return 0;
}