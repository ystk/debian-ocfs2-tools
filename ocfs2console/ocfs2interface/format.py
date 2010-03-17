# OCFS2Console - GUI frontend for OCFS2 management and debugging
# Copyright (C) 2002, 2005 Oracle.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.

import gtk

from plist import partition_list

from guiutil import Dialog, set_props, error_box, format_bytes
from process import Process

from fswidgets import BaseCombo, NumSlots, VolumeLabel, ClusterSize, BlockSize

base_command = ('mkfs.ocfs2', '-x')

class Device(BaseCombo):
    def fill(self, partitions, device):
        self.set_choices([('%s (%s)' % p, p[0] == device) for p in partitions])

    def get_device(self):
        return self.get_choice().split(' ')[0]

    label = 'Available _devices'

class FormatVolumeLabel(VolumeLabel):
    def __init__(self):
        VolumeLabel.__init__(self)
        self.set_text('oracle')

entries = (Device, FormatVolumeLabel, ClusterSize, NumSlots, BlockSize)

def format_partition(parent, device):
    partitions = []

    def add_partition(device, fstype):
        partitions.append((device, fstype))

    partition_list(add_partition, unmounted=True)

    if not partitions:
        error_box(parent, 'No unmounted partitions')
        return False

    def sort_partition(x, y):
        return cmp(x[0], y[0])

    partitions.sort(sort_partition)

    dialog = Dialog(parent=parent, title='Format',
                    buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                             gtk.STOCK_OK,     gtk.RESPONSE_OK))

    dialog.set_alternative_button_order((gtk.RESPONSE_OK, gtk.RESPONSE_CANCEL))

    dialog.set_default_response(gtk.RESPONSE_OK)

    table = gtk.Table(rows=5, columns=2)
    set_props(table, row_spacing=6,
                     column_spacing=6,
                     border_width=6,
                     parent=dialog.vbox)

    widgets = []

    for row, widget_type in enumerate(entries):
        widget = widget_type()

        label = gtk.Label()
        label.set_text_with_mnemonic(widget_type.label + ':')
        label.set_mnemonic_widget(widget)
        set_props(label, xalign=0.0)
        table.attach(label, 0, 1, row, row + 1)

        if widget_type == Device:
            widget.fill(partitions, device)

        if isinstance(widget, gtk.Entry):
            widget.set_activates_default(True)

        if isinstance(widget, gtk.SpinButton):
            attach_widget = gtk.HBox()
            attach_widget.pack_start(widget, expand=False, fill=False)
        else:
            attach_widget = widget

        table.attach(attach_widget, 1, 2, row, row + 1)

        widgets.append(widget)

    widgets[0].grab_focus()

    dialog.show_all()

    while 1:
        if dialog.run() != gtk.RESPONSE_OK:
            dialog.destroy()
            return False

        dev = widgets[0].get_device()
        msg = 'Are you sure you want to format %s?' % dev

        ask = gtk.MessageDialog(parent=dialog,
                                flags=gtk.DIALOG_DESTROY_WITH_PARENT,
                                type=gtk.MESSAGE_QUESTION,
                                buttons=gtk.BUTTONS_YES_NO,
                                message_format=msg)

        if ask.run() == gtk.RESPONSE_YES:
            break
        else:
            ask.destroy()

    command = list(base_command)

    for widget in widgets[1:]:
        arg = widget.get_arg()

        if arg:
            command.extend(arg)

    command.append(dev)

    dialog.destroy()

    mkfs = Process(command, 'Format', 'Formatting...', parent, spin_now=True)
    success, output, k = mkfs.reap()

    if not success:
        error_box(parent, 'Format error: %s' % output)
        return False

    return True

def main():
    format_partition(None, None)

if __name__ == '__main__':
    main()
