"""Shared look and shared frame for the three gizmo groups.

`orientation_frame` is the piece that matters: it returns the drawn frame and
the operator's `orient_type` together, because setting one without the other
draws handles along one set of axes while transforming along another.
"""

from mathutils import Matrix
from math import radians

AXIS_COLORS = {
    'X': (0.9, 0.15, 0.2),
    'Y': (0.35, 0.85, 0.15),
    'Z': (0.15, 0.4, 0.95),
}

# One amber, used by every handle in every group.
HIGHLIGHT_COLOR = (1.0, 0.9, 0.2)

ALPHA = 0.9
ALPHA_HIGHLIGHT = 1.0

# Local +Z is the arrow/dial/primitive's default pointing direction/normal;
# rotate it onto each world axis.
AXIS_ROTATIONS = {
    'X': Matrix.Rotation(radians(90), 4, 'Y'),
    'Y': Matrix.Rotation(radians(-90), 4, 'X'),
    'Z': Matrix.Identity(4),
}

# Points a handle down the negative side of its axis.
FLIP_ROTATION = Matrix.Rotation(radians(180), 4, 'X')


def local_basis_matrix(active_object):
    return active_object.matrix_world.to_3x3().normalized().to_4x4()


def style_handle(gz, color):
    """The colour/alpha every RoTools handle shares."""
    gz.color = color
    gz.alpha = ALPHA
    gz.color_highlight = HIGHLIGHT_COLOR
    gz.alpha_highlight = ALPHA_HIGHLIGHT
    gz.use_draw_modal = True
    return gz


def orientation_frame(context):
    """(rotation_3x3, axis_rotations, orient_type) for the scene's orientation.

    World mode gives the identity frame and `'GLOBAL'`; Local gives the active
    object's own (scale-stripped) basis and `'LOCAL'`. Returning the operator's
    orient_type alongside the drawn frame is what keeps the two in lockstep.
    """
    active = context.active_object
    if context.scene.rotools_orientation == 'LOCAL' and active is not None:
        local_basis = local_basis_matrix(active)
        return (
            local_basis.to_3x3(),
            {axis: local_basis @ rot for axis, rot in AXIS_ROTATIONS.items()},
            'LOCAL',
        )
    return Matrix.Identity(3), AXIS_ROTATIONS, 'GLOBAL'
