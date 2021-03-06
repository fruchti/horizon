#include "warp_cursor.hpp"

namespace horizon {
Coordi warp_cursor(GdkEvent *motion_event, const Gtk::Widget &widget)
{
    gdouble x, y;
    if (!gdk_event_get_coords(motion_event, &x, &y))
        return {};
    const auto w = widget.get_allocated_width();
    const auto h = widget.get_allocated_height();
    const int offset = 1;
    const bool wr = x >= w - offset;
    const bool wl = x <= offset;
    const bool wb = y >= h - offset;
    const bool wt = y <= offset;
    Coordi warp_distance;
    if (wr || wl || wb || wt) {
        auto dev = gdk_event_get_device(motion_event);
        auto srcdev = gdk_event_get_source_device(motion_event);
        const auto src = gdk_device_get_source(srcdev);
        if (src != GDK_SOURCE_PEN) {
            auto scr = gdk_event_get_screen(motion_event);
            gdouble rx, ry;
            gdk_event_get_root_coords(motion_event, &rx, &ry);
            if (wr) {
                warp_distance = Coordi(-(w - 2 * offset), 0);
            }
            else if (wl) {
                warp_distance = Coordi(+(w - 2 * offset), 0);
            }
            else if (wb) {
                warp_distance = Coordi(0, -(h - 2 * offset));
            }
            else if (wt) {
                warp_distance = Coordi(0, h - 2 * offset);
            }
            gdk_device_warp(dev, scr, rx + warp_distance.x, ry + warp_distance.y);
        }
    }
    return warp_distance;
}
} // namespace horizon
