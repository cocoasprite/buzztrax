/* $Id: song.h,v 1.10 2004-05-11 16:16:38 ensonic Exp $
 * Class for a basic buzztard song
 */
 
#ifndef BT_SONG_H
#define BT_SONG_H

#include <glib.h>
#include <glib-object.h>

#define BT_SONG_TYPE		        (bt_song_get_type ())
#define BT_SONG(obj)		        (G_TYPE_CHECK_INSTANCE_CAST ((obj), BT_SONG_TYPE, BtSong))
#define BT_SONG_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), BT_SONG_TYPE, BtSongClass))
#define BT_IS_SONG(obj)	        (G_TYPE_CHECK_TYPE ((obj), BT_SONG_TYPE))
#define BT_IS_SONG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), BT_SONG_TYPE))
#define BT_SONG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BT_SONG_TYPE, BtSongClass))

/* type macros */

typedef struct _BtSong BtSong;
typedef struct _BtSongClass BtSongClass;
typedef struct _BtSongPrivate BtSongPrivate;

struct _BtSong {
  GObject parent;
  
  /* private */
  BtSongPrivate *private;
};
/* structure of the song class */
struct _BtSongClass {
  GObjectClass parent;
  
  /* id for the play signal */
  guint play_signal_id;
  /* class methods */
  void (*start_play)(const BtSong *self);
};

/* used by SONG_TYPE */
GType bt_song_get_type(void);

#endif // BT_SONG_H

