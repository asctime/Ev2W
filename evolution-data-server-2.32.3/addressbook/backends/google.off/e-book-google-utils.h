/* e-book-google-utils.h - Google contact conversion utilities.
 *
 * Copyright (C) 2012 Philip Withnall
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Philip Withnall <philip@tecnocode.co.uk>
 */

#ifndef E_BOOK_GOOGLE_UTILS_H
#define E_BOOK_GOOGLE_UTILS_H

G_BEGIN_DECLS

/* Custom attribute names. */
#define GDATA_PHOTO_ETAG_ATTR "X-GDATA-PHOTO-ETAG"

typedef gchar *(*EContactGoogleCreateGroupFunc) (const gchar *category_name, gpointer user_data, GError **error);

GDataEntry *gdata_entry_new_from_e_contact (EContact *contact, GHashTable *groups_by_name, GHashTable *system_groups_by_id,
                                            EContactGoogleCreateGroupFunc create_group,
                                            gpointer create_group_user_data) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean gdata_entry_update_from_e_contact (GDataEntry *entry, EContact *contact, gboolean ensure_personal_group, GHashTable *groups_by_name,
                                            GHashTable *system_groups_by_id,
                                            EContactGoogleCreateGroupFunc create_group, gpointer create_group_user_data);

EContact *e_contact_new_from_gdata_entry (GDataEntry *entry, GHashTable *groups_by_id,
                                          GHashTable *system_groups_by_entry_id) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
void e_contact_add_gdata_entry_xml (EContact *contact, GDataEntry *entry);
void e_contact_remove_gdata_entry_xml (EContact *contact);
const gchar *e_contact_get_gdata_entry_xml (EContact *contact, const gchar **edit_uri);

const gchar *e_contact_map_google_with_evo_group (const gchar *group_name, gboolean google_to_evo);

gchar *e_contact_sanitise_google_group_id (const gchar *group_id) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar *e_contact_sanitise_google_group_name (GDataEntry *group) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* E_BOOK_GOOGLE_UTILS_H */
