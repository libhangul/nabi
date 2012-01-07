#include <gtk/gtk.h>

void on_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

int
main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *entry;
    GtkWidget *textview;
    GtkWidget *scrolled;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
		     G_CALLBACK(on_destroy), NULL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, TRUE, 0);

    textview = gtk_text_view_new();

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scrolled), textview);

    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
