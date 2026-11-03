"""Screen-space <-> world-space helpers shared by the dragger.

Anything the user perceives as "a few pixels" - the soft-snap margin, the
drag-by-pivot grab radius - has to be converted into world units through the
*current* view, or it behaves completely differently zoomed in than zoomed
out. Every such threshold goes through `pixels_to_world` here instead of
hardcoding a flat world-space number.
"""

from bpy_extras import view3d_utils


def mouse_ray(region, rv3d, coord):
    """World-space (origin, direction) ray through a region-relative 2D coord.

    The direction comes back normalized, which the BVH/plane intersection code
    relies on to report distances in world units.
    """
    return (
        view3d_utils.region_2d_to_origin_3d(region, rv3d, coord),
        view3d_utils.region_2d_to_vector_3d(region, rv3d, coord),
    )


def view_plane_point(region, rv3d, coord, depth_point):
    """Where the mouse ray crosses the camera-facing plane through `depth_point`."""
    return view3d_utils.region_2d_to_location_3d(region, rv3d, coord, depth_point)


def point_to_region(region, rv3d, point):
    """2D region coords of a world-space point, or None if it's behind the view."""
    return view3d_utils.location_3d_to_region_2d(region, rv3d, point)


def pixels_to_world(region, rv3d, point, pixels):
    """World-space length spanning `pixels` screen pixels at `point`'s depth.

    Derived from the window matrix rather than tuned by eye: window_matrix[1][1]
    is 1/tan(fovy/2) for a perspective view and 2/view_height for an orthographic
    one, and NDC's [-1, 1] covers region.height pixels. So one pixel spans
    2*depth / (m11 * region.height) world units, with depth pinned to 1 for
    ortho since it has no perspective divide.
    """
    m11 = rv3d.window_matrix[1][1]
    if m11 == 0.0 or region.height == 0:
        return 0.0
    depth = 1.0
    if rv3d.is_perspective:
        # View space looks down -Z, so negate to get a positive distance.
        depth = max(-(rv3d.view_matrix @ point).z, 1e-6)
    return pixels * 2.0 * depth / (m11 * region.height)
