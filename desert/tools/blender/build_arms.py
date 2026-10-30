"""Build the desert player's arms and export desert/assets/characters/arms.glb.

Run headless (builds, checks, exports, appends the clips, reads the glb back,
then renders the previews beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py
    PXE_DESERT_OUT=/some/dir ... writes the glb and previews there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.
    PXE_SKIP_EXPORT=1 ... stops after the checks.

Or step by step in an open Blender, to watch it go together:
    import runpy; am = runpy.run_path(".../build_arms.py", run_name="arms")
    b = am["Builder"](); b.start(); b.arms(); b.gloves(); b.fingers(); b.glove_detail(); b.wrap_detail()
    b.bracer(); b.gauge(); b.bracer_detail(); b.compass(); b.rig(); b.animate()
    problems, m = am["check"](b); clip_problems, cm = am["clip_checks"](b)
Each step adds its pieces to the Arms collection of the current scene, leaving
the rest of the file alone.

A view model: the wanderer's two arms, modelled round the eye at real adult
scale (desert/docs/art-style.md). The origin is the eye. The model faces -Y
in Blender (engine +Z, the view's forward), +Z up; Blender +X (engine +X) is
the player's left, so the right arm is on -X. The game places the origin at
the eye with the view's axes, so this file's frame is the view's frame.

Built in the held pose, which is the rest (bind) pose: shoulders 0.38 m apart,
0.22 m below and 0.05 m behind the eye; elbows bent to 87 degrees, 9 cm below
the shoulders, 24 cm ahead of them and 16 cm out from them, so the forearms
rise from the bottom corners of the view, forward, up and inward; wrists 0.33 m
ahead, 0.15 m below the eye and 0.20 m out from it; hands reaching 0.50 m ahead,
pointing mostly forward (8 degrees outward) on a firm, almost straight wrist
(4 degrees down), palms facing each other with the thumbs up as if loosely
holding something, fingers relaxed and a little curled, thumbs resting along
the index fingers. The gloves are chunky (GLOVE_BULK) and taper into the cuff.
With real proportions the forearm rises steeply, so only a few centimetres
behind the glove show; the cuff is kept short and the bracer's gauge sits at
its wrist end, the plate reaching under the cuff, so the gauge is in view.
The build checks the framing through the eye camera (60 degree vertical field
of view, 16:9), the near clearance and the gauge, and fails if any is off.

Every piece is a closed volume (the engine culls back faces), flat-shaded,
one solid colour per material. A view model carries detail down to about
3 px on screen (desert/docs/art-style.md, "View models are the exception"):
    arm tube      Arms_Wrap   shoulder cap to wrist; the forearm wrapped in
                              six stepped cloth bands, each lip standing
                              4-6 mm proud of the band under it, uneven in
                              width and angle
                  Arms_Grime  the 2nd and 5th band from the wrist; a frayed
                              tail of three strands from the band nearest
                              the cuff, on the forearm's outer side
    glove         Arms_Glove  a flared cuff over the wraps, a faceted hand,
                              four fingers and a thumb in blocky 12-sided
                              segments that overlap at the knuckles
                  Arms_Thread stitch dashes along both sides of every finger
                              and thumb, and round the cuff's flare
                  Arms_Worn   knuckle pads, a palm patch, a back-of-hand
                              patch and a patch on every fingertip
                  Arms_Strap  a strap round each cuff, Arms_Steel snap rivet
    bracer (left) Arms_Brass  a bevelled brass plate in four facets
                  Arms_Strap  two straps, Arms_Thread stitched edges, each
                              buckle an Arms_Steel frame and tongue and an
                              Arms_Strap keeper loop
    gauge (left)  Arms_Brass  a bezel on the plate's wrist end, turned toward
                              the eye, its window recessed 3 mm, long side
                              level across the view
                  Arms_Screen a backlit panel on the window floor (emissive)
                  Arms_Glow   the readout: 47% and a block cursor in lit
                              2.4 mm dots (emissive)
                  Arms_Steel  four slotted screws on the bezel's corners; a
                              knurled dial on its inner side (Arms_Paint
                              index mark); three toggles at the plate's wrist
                              end (steel base, Arms_Rubber lever, Arms_Paint
                              tip); a canister with a clip and valve wheel
                  Arms_Copper a coil round a short brass tube, and a cable
                  Arms_Rubber a second cable; both tuck under the wrap band
                              past the plate's elbow end
    compass       Arms_Strap  a strap over the middle of the right cuff,
    (right)                   carrying an Arms_Steel post, housing, hinged
                              sun-shade lid propped open, and Arms_Rubber
                              side buttons
                  Arms_Dial   the face, 30 mm across, turned toward the eye
                  Arms_Paint  12 tick marks and a fixed needle
                  Arms_Amber  the status lamp on the face's rim (emissive)

One skinned mesh (`Arms`) on one armature (`Arms_Rig`), 39 joints:
    root
    shoulder > upper_arm > forearm > hand              (.L / .R)
    hand > thumb.1 > thumb.2 > thumb.3, and the same for index, middle,
    ring and pinky
Weights blend over 4 cm across each elbow and wrist; every finger segment is
rigid to its joint, and every gadget part (bracer, gauge, straps, devices,
compass and its mount) to its forearm. Stitches, pads and patches take the
weights of the glove surface under them. The attachment check runs in the
rest pose and in test poses: each wrist twisted, flexed and bent sideways 30
degrees each way, every finger and thumb joint curled 60 degrees, each elbow
opened 30 degrees and closed 10.

Two looping clips (task desert-arms-walk), a first-person carry in place,
each 1.0 s at 30 fps, 31 keys (the last the first), linear, every joint keyed
on every frame: Arms_Walk (2.0 m walked per cycle) and Arms_Sprint (3.0 m).
Both hands bob twice per cycle and counter-swing once, the left furthest
forward at phase 0, the bob's low points at phases 0 and 0.5; each forearm
rocks at the elbow with its swing and the hand trails it at the wrist; the
shoulder and upper arm turn the least they can to put each wrist on its
target. Arms_Sprint bobs the hands 20 mm (no lift), swings them wider and
curls every finger and thumb joint 25 degrees into a loose fist. Only
shoulder, upper_arm, forearm and hand turn (and the digits in the sprint);
every other joint, and every translation and scale, is keyed at exactly
rest. They are kept as actions on the rig, and the build appends them to the
exported glb itself (add_clips), so the rest keys are exact. The checks
(clip_checks, check_glb_clips) sample every key and half-key as the engine
does.

Material order is the contract with the engine: glTF material i is actor
slot firstMaterial + i (desert::Arms loads into slots 0-14).
"""

import json
import math
import os
import random
import struct

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_DESERT_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "characters"))
OUT = os.path.join(OUT_DIR, "arms.glb")
PREVIEW = os.path.join(OUT_DIR, "arms.png")
PREVIEW_EYE = os.path.join(OUT_DIR, "arms_eye.png")
PREVIEW_LEFT = os.path.join(OUT_DIR, "arms_left.png")
PREVIEW_RIGHT = os.path.join(OUT_DIR, "arms_right.png")
PREVIEW_CLIPS = {"Arms_Walk": os.path.join(OUT_DIR, "arms_walk.png"),
                 "Arms_Sprint": os.path.join(OUT_DIR, "arms_sprint.png")}
COLLECTION = "Arms"
MESH = "Arms"
RIG = "Arms_Rig"

X = Vector((1, 0, 0))
Y = Vector((0, 1, 0))
Z = Vector((0, 0, 1))


def v(x, y, z):
    return Vector((x, y, z))


def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def to_srgb(c):
    """The engine's conversion back to display values (skinned.cpp toSrgb)."""
    c = max(0.0, min(1.0, c))
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055


def clamp01(x):
    return max(0.0, min(1.0, x))


def smoothstep(a, b, x):
    t = clamp01((x - a) / (b - a))
    return t * t * (3.0 - 2.0 * t)


def rot(axis, deg, vec):
    return Matrix.Rotation(math.radians(deg), 3, Vector(axis).normalized()) @ Vector(vec)


def mirror(p):
    return Vector((-p.x, p.y, p.z))


# --------------------------------------------------------------------------
# Materials, in the engine's order: (name, sRGB mid tone, emissive).

MATERIALS = [
    ("Arms_Wrap", (206, 194, 168), False),   # bleached cloth bands
    ("Arms_Glove", (78, 54, 40), False),     # dark leather gloves
    ("Arms_Strap", (118, 84, 56), False),    # bracer straps and buckles
    ("Arms_Brass", (176, 132, 64), False),   # worn brass bracer plate and bezel
    ("Arms_Glow", (70, 255, 110), True),     # the gauge readout's lit dots (phosphor green)
    ("Arms_Steel", (92, 96, 102), False),    # screws, canister, valve, compass housing and lid, buckles, rivets, toggle bases
    ("Arms_Copper", (178, 98, 56), False),   # coil, copper cable
    ("Arms_Rubber", (44, 40, 38), False),    # rubber cable, toggle levers, compass buttons
    ("Arms_Thread", (132, 100, 70), False),  # glove and strap stitching
    ("Arms_Worn", (112, 82, 62), False),     # knuckle pads, palm, back-of-hand and fingertip patches
    ("Arms_Grime", (160, 146, 120), False),  # grime bands, frayed tails
    ("Arms_Dial", (222, 212, 186), False),   # the compass face (ivory enamel)
    ("Arms_Paint", (170, 64, 44), False),    # toggle tips, compass needle and ticks, dial index mark
    ("Arms_Amber", (255, 170, 60), True),    # the compass's status lamp
    ("Arms_Screen", (30, 46, 36), True),     # the gauge's backlit screen panel
]
M = {name: i for i, (name, _, _) in enumerate(MATERIALS)}
TRI_BUDGET = (12000, 20000)
# The materials the game inks creases on: a round part in one of these takes
# at least ROUND_MIN sides (36 degrees between them, under the 40 degree ink turn).
CREASED = ("Arms_Wrap", "Arms_Glove", "Arms_Brass", "Arms_Worn", "Arms_Grime")
ROUND_MIN = 10

# --------------------------------------------------------------------------
# The eye camera the framing is checked through, as the engine's view.

FOV_Y = math.radians(60.0)
ASPECT = 16.0 / 9.0
NEAR = 0.1
TAN_Y = math.tan(FOV_Y / 2.0)
VIEW_W, VIEW_H = 960, 540

FRAME_TOP = -0.30            # nothing in view above this NDC y (the lower 35%)
KNUCKLE_Y = (-0.70, -0.40)   # top of each index knuckle, NDC y
HAND_X = (0.40, 0.60)        # each hand's centre, |NDC x|
CLEARANCE = 0.15             # every vertex in view at least this far ahead (m)
SMALL_MIN_PX = 3.0           # every seen small part (screw head, toggle tip, lamp, needle, tick, index mark), and the knurl pitch
FACE_MIN_PX = 30.0           # the compass dial at least this wide on screen
FACE_COS = 0.87              # the compass face's normal within 30 degrees of the direction to the eye
SEEN_TOL = 0.0005            # a vertex is seen when the first surface toward it is within this of it (m)


def project(p):
    """NDC x (screen right +), NDC y (up +) and depth along the view, or None
    behind the eye."""
    d = -p.y
    if d <= 1e-9:
        return None
    return (-p.x / (d * TAN_Y * ASPECT), p.z / (d * TAN_Y), d)


def in_view(p):
    q = project(p)
    return q is not None and abs(q[0]) <= 1.0 and abs(q[1]) <= 1.0


def to_px(q):
    """NDC to pixels at VIEW_W x VIEW_H (origin at the centre)."""
    return (q[0] * VIEW_W / 2, q[1] * VIEW_H / 2)


def screen_width(points):
    """A part's width on screen: the smallest extent of its in-view points,
    projected at VIEW_W x VIEW_H, over 8 screen directions 22.5 degrees apart.
    None when nothing is in view."""
    px = [to_px(project(p)) for p in points if in_view(p)]
    if not px:
        return None
    best = float("inf")
    for k in range(8):
        a = math.radians(22.5 * k)
        c, s = math.cos(a), math.sin(a)
        vals = [x * c + y * s for x, y in px]
        best = min(best, max(vals) - min(vals))
    return best


def first_hit(tree, p):
    """The first surface a ray from the eye toward p meets, up to just past p:
    (location, polygon index), or (None, None)."""
    d = p.length
    hit, _, idx, _ = tree.ray_cast(Vector((0, 0, 0)), p / d, d + 0.002)
    return hit, idx


def seen(tree, p):
    """In view, and the first surface a ray from the eye toward p meets is within SEEN_TOL of it."""
    if not in_view(p):
        return False
    hit, _ = first_hit(tree, p)
    return hit is not None and (hit - p).length <= SEEN_TOL


# --------------------------------------------------------------------------
# The held pose, for the left arm (+X); the right arm mirrors it.

SHOULDER = v(0.19, 0.05, -0.22)     # 0.38 m apart, 0.22 below and 0.05 behind the eye
CLAVICLE = v(0.03, 0.075, -0.205)   # the shoulder joint's head, at the base of the neck
UPPER_LEN = 0.30
FORE_LEN = 0.26
WRIST = v(0.200, -0.330, -0.150)    # where the forearm meets the hand
ELBOW_SWING = 48.0                  # degrees the elbow swings out from straight below the shoulder-wrist line
HAND_YAW = -8.0                     # the hand turns this far outward (degrees; negative is outward)
HAND_PITCH = 4.0                    # and this far down: a firm, almost straight wrist
HAND_ROLL = 90.0                    # the palm turns from facing down to facing inward: palms face each other, thumbs up

# Forearm (cloth over it): radius at the wrist and at the elbow.
FORE_R0, FORE_R1 = 0.030, 0.044
UPPER_R = 0.050
SIDES = 12        # 30 degrees between sides: under the 40 degree crease ink
BAND_COUNT = 6
BAND_START = -0.026            # the first band's foot, under the cuff (m from the wrist along the forearm)
BAND_W = (0.024, 0.030)        # band widths
BAND_H = (0.0042, 0.0058)      # how far each lip stands proud of the band under it
BAND_TILT = (1.5, 4.5)         # degrees each band edge leans
BEVEL = math.radians(60.0)     # each lip's bevel down to the band under it, from the axis
CREASE_MIN = 40.0              # each band edge must turn more than this

# Glove cuff: (s from the wrist along the forearm, radius); the first is the rear face.
CUFF = [(-0.036, 0.045), (-0.031, 0.046), (-0.019, 0.041), (-0.004, 0.036), (0.012, 0.034)]
# The gloves' bulk: every glove width and thickness (PALM's half width and
# heights, each finger's w0 and t0, THUMB_W and THUMB_T) is scaled by
# GLOVE_BULK where it is read. PALM's first ring is spared, so the glove
# tapers into the cuff. Lengths, curls, splays and palm_front's reach don't change.
GLOVE_BULK = 1.15
# Hand: (s along the hand, half width, dorsal height, volar height; the last
# three before GLOVE_BULK). The last ring is the knuckle line, slanted (see palm_front).
PALM = [(-0.012, 0.023, 0.011, 0.011), (0.005, 0.030, 0.0140, 0.0125), (0.022, 0.037, 0.0165, 0.014),
        (0.040, 0.041, 0.0175, 0.0137), (0.058, 0.0435, 0.0175, 0.013), (0.071, 0.0445, 0.0160, 0.0120),
        (None, 0.0445, 0.0135, 0.0105)]
# (lateral, height) round the hand, thumb side first; the dorsal side is k 1-6, the volar side k 8-11.
PALM_SHAPE = [(1.0, 0.05), (0.8, 0.62), (0.45, 0.98), (0.12, 1.08), (-0.22, 1.05), (-0.55, 0.93),
              (-0.85, 0.55), (-1.0, 0.0), (-0.8, -0.72), (-0.4, -1.0), (0.1, -1.02), (0.6, -0.9)]
HAND_LEN = 0.088   # wrist to the middle knuckle


def palm_front(t):
    """The knuckle line's reach along the hand at lateral offset t (thumb side +)."""
    return 0.084 + 0.075 * t


# Fingers: name, lateral offset (thumb side +), dorsal offset, splay (degrees,
# toward the thumb +), segment lengths, rest curls (degrees, each joint),
# width at the base, thickness at the base (both before GLOVE_BULK).
FINGERS = [
    ("index", 0.0315, 0.002, 2.5, (0.043, 0.025, 0.021), (12.0, 20.0, 13.0), 0.0210, 0.0175),
    ("middle", 0.0105, 0.003, 0.5, (0.047, 0.028, 0.023), (15.0, 23.0, 15.0), 0.0215, 0.0180),
    ("ring", -0.0105, 0.002, -2.0, (0.044, 0.027, 0.022), (18.0, 26.0, 16.0), 0.0210, 0.0172),
    ("pinky", -0.0305, 0.000, -5.0, (0.035, 0.021, 0.019), (22.0, 29.0, 18.0), 0.0180, 0.0150),
]
OVERLAP = 0.004   # each segment reaches this far past its joints, so knuckles overlap
# Thumb: base (along hand, lateral, dorsal), first direction (f, t, n), segment
# lengths, bends toward the index (degrees), widths, thicknesses (before GLOVE_BULK).
THUMB_BASE = (0.020, 0.022, -0.011)
THUMB_DIR = (0.80, 0.40, -0.45)
THUMB_LEN = (0.046, 0.033, 0.027)
THUMB_BEND = (0.0, 30.0, 16.0)
THUMB_W = (0.026, 0.0205, 0.0185)
THUMB_T = (0.022, 0.0180, 0.0160)

# Bracer, left forearm: along the forearm (s from the wrist), angular span
# about the back of the forearm, thickness.
BRACER_S = (-0.160, -0.020)   # the wrist end reaches under the cuff, so the toggles stay on the plate
BRACER_SPAN = 70.0     # degrees either side of the back of the forearm: four 35 degree facets
BRACER_T = 0.004
BRACER_SIT = 0.0045    # the plate's inside sits this far out from a band foot, into the lips
STRAPS = (-0.148, -0.090)   # strap centres along the forearm, behind the gauge
STRAP_W = 0.014
STRAP_T = 0.003
BUCKLE = (0.013, 0.013, 0.0035)
# Gauge: centre along the forearm, angle round from the back of the forearm
# toward the eye side, how far its face turns from the plate toward the eye.
GAUGE_S = -0.035
GAUGE_TURN = 0.55
WINDOW = (0.045, 0.022)
BEZEL = 0.005
RECESS = 0.003
# The gauge's screen: a backlit panel on the window floor (across, up the
# forearm, and how far it stands off the floor, sunk as far into it), and a
# readout of lit dots on it in a 3x5 font: square dots this wide, standing
# LCD_PROUD off the panel (sunk 0.4 mm into it). One blank column between
# glyphs. Each glyph is five rows, top first, X lit.
LCD_PANEL = (0.044, 0.021, 0.0006)
LCD_DOT = 0.0024
LCD_PROUD = 0.0010
LCD_TEXT = (
    ("X.X", "X.X", "XXX", "..X", "..X"),   # 4
    ("XXX", "..X", "..X", ".X.", ".X."),   # 7
    ("X.X", "..X", ".X.", "X..", "X.X"),   # %
    ("XXX", "XXX", "XXX", "XXX", "XXX"),   # the block cursor
)
LCD_MIN_PX = 3.0             # each dot at least this wide on screen, both ways

# ---- detail (desert-arms-detail) -------------------------------------------
# Glove stitching: dash length, width, gap, and how far it stands proud (and sinks in).
STITCH = (0.005, 0.0025, 0.0025, 0.0006, 0.0004)
CUFF_SEAM = 0.7         # the cuff seam's place across the flare, from its rear ring (0) to its front ring (1)
PAD = (0.012, 0.010, 0.0025, 0.001)   # knuckle pad: across, along, proud, bevel
PATCH = (0.0008, 0.0006)              # worn patches: proud, sunk
CUFF_STRAP = (-0.014, -0.004, -0.002)  # the cuff strap's rear edge, the cuff ring under it, its front edge
STRAP_PROUD = 0.0024    # straps over the glove stand this far out, and sink 0.6 mm in
# Wrap tails: the band they hang from (0 = nearest the wrist), where along it,
# strand lengths, width and thickness.
GRIME_BANDS = (1, 4)    # the 2nd and 5th band from the wrist
TAIL_S = -0.046
TAIL_LEN = (0.088, 0.074, 0.064)
TAIL_W, TAIL_T = 0.0055, 0.0016
# Bracer devices.
PLATE_BEVEL = 0.0015
SCREW = (0.005, 0.0012)            # head across, slot width
SCREW_INSET = 0.0025               # head centre in from the bezel's edges
KNURL = (0.0065, 0.0058, 16, 0.003)  # ridge radius, valley radius, ridges, thickness
KNURL_TURN = 0.75                  # the dial's axis turns this far from the bezel's inner side toward the eye
TOGGLE_W = (-0.018, -0.010, -0.002)  # toggle centres across the bezel (its long side; - is the outer side)
TOGGLE_TIP = 0.003
COIL = (-0.1325, -0.1075, 8, 28.0)  # coil from/to (m from the wrist), turns, angle round the plate
CANISTER = (-0.137, -0.087, 0.009, -76.0)  # elbow end, wrist end, radius, angle round the forearm
CABLES = (("copper", "Arms_Copper", -0.008, 0.0013), ("rubber", "Arms_Rubber", 0.002, 0.0015))
CABLE_END = -0.180      # each cable's end, tucked inside the wrap band past the plate's elbow end
# The sun compass, right forearm.
COMPASS_STRAP = (-0.0155, -0.011, -0.0065)  # strap rear edge, the post's line, front edge
COMPASS_POST = 0.008
COMPASS_TURN = 0.5      # the face turns this far from the post's line toward the eye
FACE_R = 0.015          # the face is 30 mm across
HOUSING_R = 0.018
LID_OPEN = 112.0        # degrees the lid stands open from closed
TICK = (0.0116, 0.0032, 0.0026)   # tick centre radius, radial length, width
NEEDLE = (0.0105, 0.006, 0.0034, 50.0)  # tip radius, tail radius, width, direction round the face
LAMP = (0.0131, 0.0018, 135.0)    # centre radius, radius, direction round the face

# ---- clips (desert-arms-walk) ----------------------------------------------
# A first-person carry, in place: both hands bob twice per stride cycle and
# counter-swing fore and aft once, the left forward at phase 0; the forearms
# rock at the elbow with the swing and the hands trail it at the wrist.
# Phase is time / 1.0 s. The game drives it by the distance walked: `cycle`
# metres per cycle (two steps). Offsets are each hand joint's head from rest,
# engine y (up) and z (forward); angles in degrees.
FPS = 30
CLIP_FRAMES = 30          # 1.0 s: 31 keys, the last equal to the first
CLIPS = {
    "Arms_Walk": dict(cycle=2.0, bob=0.015, swing=0.025, lift=0.0, elbow=4.0, wrist=6.0, fist=0.0),
    "Arms_Sprint": dict(cycle=3.0, bob=0.020, swing=0.050, lift=0.0, elbow=8.0, wrist=6.0, fist=25.0),
}
CLIP_ORDER = ("Arms_Walk", "Arms_Sprint")
# What the checks hold the clips to (task desert-arms-walk, Assets), kept
# apart from CLIPS on purpose so a slip in the motion's numbers fails the
# build: each hand's y offset reaches mean +- bob, its z offset +- swing (m).
CLIP_TARGETS = {
    "Arms_Walk": dict(mean=0.0, bob=0.015, swing=0.025),
    "Arms_Sprint": dict(mean=0.0, bob=0.020, swing=0.050),
}
LIMBS = ("shoulder", "upper_arm", "forearm", "hand")   # the joints a clip may turn
DIGITS = ("thumb", "index", "middle", "ring", "pinky")  # and, curled into a fist, these in Arms_Sprint
SOLVE_WEIGHT = (1.0, 1.0)  # how hard the shoulder and the upper arm resist turning when placing the wrist
AMP_TOL = 0.003           # each offset's reach within this of the spec (m)
TIMING_TOL = 1.0 / 30.0   # each peak within this of its phase
WRIST_MAX = 30.0          # the test poses' ranges from rest: a hand about each axis,
FINGER_MAX = 60.0         # a finger or thumb joint,
ELBOW_SPAN = (-10.0, 30.0)  # an elbow closed (-) or opened (+)
STEP_MAX = 45.0           # no joint turns more than this between adjacent keys
ELBOW_CLOSE_TEST = 10.0   # the closed-elbow test pose
ATTACH_FRAMES = 8         # attachment is checked at this many evenly spaced phases of each clip
SHEET = (4, 2, 640, 360)  # clip preview sheets: columns, rows, tile size


def ring_frame(t, ref):
    t = t.normalized()
    f = ref - t * ref.dot(t)
    if f.length < 1e-5:
        f = Z - t * Z.dot(t)
    f.normalize()
    return f.cross(t).normalized(), f


def circle(c, axis, r, n, ref=X, phase=0.5):
    """n points round c, square to axis, radius r."""
    u, w = ring_frame(axis, ref)
    out = []
    for k in range(n):
        a = 2 * math.pi * (k + phase) / n
        out.append(c + (u * math.cos(a) + w * math.sin(a)) * r)
    return out


def section_pts(c, side, up, w, th, grow=0.0):
    """A blocky segment's cross-section: a w x th box with its corners cut, 12
    sides with 30 degrees between them (under the 40 degree crease ink), grown
    outward by `grow` all round. The side flats sit at +-w/2, the top and
    bottom at +-th/2."""
    a, cc = w / 2, th / 2
    rho = 0.4 * min(a, cc)
    hs = []
    for k in range(12):
        t = math.radians(30 * k)
        hs.append((a - rho) * abs(math.cos(t)) + (cc - rho) * abs(math.sin(t)) + rho + grow)
    pts = []
    for k in range(12):
        t1, t2 = math.radians(30 * k), math.radians(30 * (k + 1))
        h1, h2 = hs[k], hs[(k + 1) % 12]
        x = (h1 * math.sin(t2) - h2 * math.sin(t1)) / 0.5
        y = (math.cos(t1) * h2 - math.cos(t2) * h1) / 0.5
        pts.append(c + side * x + up * y)
    return pts


def clip_half(poly, nrm, off):
    """The part of a convex 2D polygon with p . nrm >= off (one Sutherland-Hodgman pass)."""
    out = []
    for i, p in enumerate(poly):
        q = poly[(i + 1) % len(poly)]
        dp = p[0] * nrm[0] + p[1] * nrm[1] - off
        dq = q[0] * nrm[0] + q[1] * nrm[1] - off
        if dp >= 0:
            out.append(p)
        if (dp >= 0) != (dq >= 0):
            t = dp / (dp - dq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


class Side:
    """The held pose's joints and frames for one arm, in world space."""

    def __init__(self, tag):
        self.tag = tag
        mir = tag == "R"
        P = mirror if mir else (lambda p: Vector(p))
        S, W = SHOULDER.copy(), WRIST.copy()
        # Elbow: where the 0.30 upper arm and 0.26 forearm meet, swung out
        # from straight below the shoulder-wrist line.
        d = W - S
        L = d.length
        d.normalize()
        a = (UPPER_LEN ** 2 - FORE_LEN ** 2 + L ** 2) / (2 * L)
        r = math.sqrt(max(UPPER_LEN ** 2 - a * a, 0.0))
        down = (-Z - d * (-Z).dot(d)).normalized()
        out = d.cross(down).normalized()
        if out.x < 0:
            out = -out
        sw = math.radians(ELBOW_SWING)
        E = S + d * a + (down * math.cos(sw) + out * math.sin(sw)) * r
        # Hand frame: f forward, n the back of the hand, t toward the thumb.
        yaw, pitch = math.radians(HAND_YAW), math.radians(HAND_PITCH)
        f = v(-math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch))
        n0 = (Z - f * Z.dot(f)).normalized()
        o = n0.cross(f).normalized()   # with f forward and n0 up, this points outward (+X) for the left
        if o.x < 0:
            o = -o
        roll = math.radians(HAND_ROLL)
        n = (n0 * math.cos(roll) + o * math.sin(roll)).normalized()
        t = f.cross(n).normalized()    # toward the thumb: inward (-X) on the left hand
        self.S, self.E, self.W = P(S), P(E), P(W)
        self.C = P(CLAVICLE)
        self.f, self.n, self.t = P(f), P(n), P(t)
        self.au = (self.E - self.S).normalized()
        self.af = (self.W - self.E).normalized()
        self.K_mid = self.W + self.f * HAND_LEN + self.n * 0.003
        # Fingers: joint positions and per-segment frames.
        self.fingers = {}
        for name, lat, dors, splay, lens, curls, w0, t0 in FINGERS:
            reach = palm_front(lat) - 0.004
            K = self.W + self.f * reach + self.t * lat + self.n * dors
            d0 = rot(self.n, splay if not mir else -splay, self.f)
            hinge = self.n.cross(d0).normalized()      # curls toward the palm for a positive angle
            pts, frames = [K], []
            dirv, up = d0.copy(), self.n.copy()
            for L_, c in zip(lens, curls):
                dirv = rot(hinge, c, dirv)
                up = rot(hinge, c, up)
                frames.append((dirv.copy(), up.copy()))
                pts.append(pts[-1] + dirv * L_)
            self.fingers[name] = (pts, frames, hinge, w0 * GLOVE_BULK, t0 * GLOVE_BULK)
        # Thumb.
        fb, tb, nb = THUMB_BASE
        T0 = self.W + self.f * fb + self.t * tb + self.n * nb
        fd, td, nd = THUMB_DIR
        dirv = (self.f * fd + self.t * td + self.n * nd).normalized()
        side_ax = self.n.cross(dirv).normalized()
        up = dirv.cross(side_ax).normalized()
        if up.dot(self.n) < 0:
            up = -up
        pts, frames = [T0], []
        for L_, bend in zip(THUMB_LEN, THUMB_BEND):
            # Each joint bends the thumb toward the index finger's line.
            ax = dirv.cross(self.f)
            if ax.length > 1e-6 and bend:
                dirv = rot(ax, bend, dirv)
                up = rot(ax, bend, up)
            frames.append((dirv.copy(), up.copy()))
            pts.append(pts[-1] + dirv * L_)
        self.thumb = (pts, frames, self.n.cross(frames[0][0]).normalized())
        # The top of the forearm, where the bracer sits: square to the
        # forearm, halfway between straight up and toward the eye from the gauge.
        g = self.on_forearm(GAUGE_S)
        up = Z + (-g).normalized()
        self.back = (up - self.af * up.dot(self.af)).normalized()

    def s_wrist(self, p):
        return (p - self.W).dot(self.af)

    def s_elbow(self, p):
        b = (self.au + self.af).normalized()
        return (p - self.E).dot(b)

    def on_forearm(self, s):
        """The forearm axis point s metres from the wrist (s < 0 toward the elbow)."""
        return self.W + self.af * s

    def fore_r(self, s):
        return FORE_R0 + (FORE_R1 - FORE_R0) * clamp01(-s / FORE_LEN)


def bone_names():
    names = ["root"]
    for S in "LR":
        names += [f"shoulder.{S}", f"upper_arm.{S}", f"forearm.{S}", f"hand.{S}"]
        for digit in ("thumb", "index", "middle", "ring", "pinky"):
            names += [f"{digit}.{i}.{S}" for i in (1, 2, 3)]
    return names


JOINTS = bone_names()


class Piece:
    def __init__(self, name, mat):
        self.name = name
        self.mat = mat
        self.verts = []
        self.faces = []
        self.sides = None     # modelled round: the number of sides round it
        self.gadget = None    # a gadget part: the forearm joint it is rigid to


# --------------------------------------------------------------------------
# The builder: one bmesh, one mesh object, weights in the deform layer


class Builder:
    STEPS = ["start", "arms", "gloves", "fingers", "glove_detail", "wrap_detail", "bracer", "gauge",
             "bracer_detail", "compass", "rig", "animate"]

    def __init__(self):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.group = {name: i for i, name in enumerate(JOINTS)}
        self.sides = {S: Side(S) for S in "LR"}
        self.pieces = {}
        self.attach = []     # (piece, piece) that must touch in every pose
        self.covered = []    # (piece, vertex list, [pieces]): verts that must stay inside one of the pieces
        self.band_edges = []  # per forearm, ring loops whose edges must crease
        self.lcd = []        # (piece, glyph, row, top centre, right, up, n): every lit dot of the gauge's readout
        self.segs = {}       # (side, digit, segment) -> the segment's frame and sizes, for the glove detail
        self.palm_loops = {}
        self.cuff_loops = {}
        self.stitches = []   # (piece, along, across): every stitch dash, for the stitching check
        self.screws = []     # the bezel's screw pieces
        self.toggles = []    # (base, lever, tip) pieces
        self.knurl = []      # each knurl ridge's centre on the dial's front edge
        self.compass_info = {}
        self.obj = None
        self.rig_obj = None
        self.coll = None

    # ---- scene plumbing ---------------------------------------------------

    def start(self):
        """A fresh Arms collection and the 15 materials; the rest of the file is left alone."""
        old = bpy.data.collections.get(COLLECTION)
        if old:
            for ob in list(old.objects):
                data = ob.data
                bpy.data.objects.remove(ob)
                if isinstance(data, bpy.types.Mesh):
                    bpy.data.meshes.remove(data)
                elif isinstance(data, bpy.types.Armature):
                    bpy.data.armatures.remove(data)
            bpy.data.collections.remove(old)
        self.coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(self.coll)
        mats = []
        for name, srgb, glow in MATERIALS:
            colour = tuple(lin(c) for c in srgb)
            mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
            mat.diffuse_color = (*colour, 1.0)
            if mat.node_tree is None:
                mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Roughness"].default_value = 1.0
                bsdf.inputs["Metallic"].default_value = 0.0
                bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Emission Strength"].default_value = 1.0 if glow else 0.0
            mats.append(mat)
        mesh = bpy.data.meshes.new(MESH)
        for mat in mats:
            mesh.materials.append(mat)
        self.obj = bpy.data.objects.new(MESH, mesh)
        self.coll.objects.link(self.obj)
        for name in JOINTS:
            self.obj.vertex_groups.new(name=name)
        self.refresh()

    def refresh(self):
        self.bm.to_mesh(self.obj.data)
        self.obj.data.update()

    # ---- geometry helpers -------------------------------------------------

    def piece(self, name, mat):
        p = Piece(name, mat)
        self.pieces[name] = p
        return p

    def loft(self, piece, loops, cap0=True, cap1=True):
        """Faces between consecutive loops of points (each the same length),
        capped flat at the ends. Returns the BMVert loops."""
        bm = self.bm
        vl = [[bm.verts.new(Vector(p)) for p in loop] for loop in loops]
        for loop in vl:
            piece.verts += loop
        n = len(vl[0])
        for a, b in zip(vl, vl[1:]):
            for k in range(n):
                j = (k + 1) % n
                piece.faces.append(bm.faces.new((a[k], a[j], b[j], b[k])))
        if cap0:
            piece.faces.append(bm.faces.new(list(reversed(vl[0]))))
        if cap1:
            piece.faces.append(bm.faces.new(vl[-1]))
        return vl

    def outward(self, piece):
        """Wind a closed piece consistently, then turn its faces outward (positive volume)."""
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=piece.faces)
        bm.normal_update()
        vol = 0.0
        for f in piece.faces:
            co = [x.co for x in f.verts]
            for i in range(1, len(co) - 1):
                vol += co[0].dot(co[i].cross(co[i + 1]))
        if vol < 0:
            for f in piece.faces:
                f.normal_flip()
            bm.normal_update()

    def finish(self, piece, weights):
        """Material, flat shading, outward faces, and weights(point) -> [(joint, w)]."""
        self.outward(piece)
        for f in piece.faces:
            f.material_index = M[piece.mat]
            f.smooth = False
        for x in piece.verts:
            self.set_weights(x, weights(x.co))

    def set_weights(self, x, ws):
        acc = {}
        for n, w in ws:
            acc[n] = acc.get(n, 0.0) + w
        items = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        total = sum(w for _, w in items) or 1.0
        x[self.deform].clear()
        for n, w in items:
            if w / total > 1e-3:
                x[self.deform][self.group[n]] = w / total

    def box_loops(self, a, b, d, up, w, th, wb=None, tb=None):
        """Two rectangular loops from a to b along d, square to `up`."""
        side = d.cross(up).normalized()
        up = side.cross(d).normalized()
        wb = w if wb is None else wb
        tb = th if tb is None else tb

        def rect(c, ww, tt):
            return [c + side * (sx * ww / 2) + up * (sy * tt / 2) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        return rect(a, w, th), rect(b, wb, tb)

    def ring_loft(self, piece, loops):
        """Faces between consecutive loops, the last joined back to the first:
        a closed ring (a torus) with no caps."""
        bm = self.bm
        vl = [[bm.verts.new(Vector(q)) for q in loop] for loop in loops]
        for loop in vl:
            piece.verts += loop
        m, n = len(vl), len(vl[0])
        for i in range(m):
            A, B = vl[i], vl[(i + 1) % m]
            for k in range(n):
                j = (k + 1) % n
                piece.faces.append(bm.faces.new((A[k], A[j], B[j], B[k])))
        return vl

    def box(self, piece, c, ax, ay, az, sx, sy, sz):
        """A box centred on c, sx x sy x sz along the axes ax, ay, az."""
        az = az.normalized()
        ax = (ax - az * ax.dot(az)).normalized()
        ay = az.cross(ax).normalized()   # the box is symmetric, so ay's sign doesn't matter
        la = [c - az * (sz / 2) + ax * (i * sx / 2) + ay * (j * sy / 2) for i, j in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        lb = [q + az * sz for q in la]
        return self.loft(piece, [la, lb])

    def cylinder(self, piece, a, b, r, n, ref=X, rb=None, bevel=0.0):
        """A closed n-sided cylinder from a to b (radius r, rb at b), its ends
        chamfered by `bevel`."""
        axis = (b - a).normalized()
        rb = r if rb is None else rb
        if bevel:
            loops = [circle(a, axis, r - bevel, n, ref), circle(a + axis * bevel, axis, r, n, ref),
                     circle(b - axis * bevel, axis, rb, n, ref), circle(b, axis, rb - bevel, n, ref)]
        else:
            loops = [circle(a, axis, r, n, ref), circle(b, axis, rb, n, ref)]
        self.loft(piece, loops)
        piece.sides = n

    def frame(self, piece, c, a1, a2, nrm, o1, o2, bar, th):
        """A rectangular frame (a ring of square section) round c in the a1-a2
        plane: outer size o1 x o2, bars `bar` wide, `th` thick along nrm."""
        def rect(h1, h2, z):
            return [c + a1 * (i * h1) + a2 * (j * h2) + nrm * z for i, j in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        self.ring_loft(piece, [rect(o1 / 2, o2 / 2, -th / 2), rect(o1 / 2, o2 / 2, th / 2),
                               rect(o1 / 2 - bar, o2 / 2 - bar, th / 2), rect(o1 / 2 - bar, o2 / 2 - bar, -th / 2)])

    def tube(self, piece, path, r, n):
        """An n-sided tube along a path of points, square to it at every point
        (parallel-transported, so it doesn't twist), capped at both ends."""
        loops = []
        t0 = (path[1] - path[0]).normalized()
        u = Z - t0 * Z.dot(t0)
        if u.length < 1e-4:
            u = X - t0 * X.dot(t0)
        u.normalize()
        for i, c in enumerate(path):
            if i == 0:
                t = t0
            elif i == len(path) - 1:
                t = (path[-1] - path[-2]).normalized()
            else:
                t = (path[i + 1] - path[i - 1]).normalized()
            u = (u - t * u.dot(t)).normalized()
            w = t.cross(u)
            loops.append([c + (u * math.cos(2 * math.pi * (k + 0.5) / n) + w * math.sin(2 * math.pi * (k + 0.5) / n)) * r
                          for k in range(n)])
        vl = self.loft(piece, loops)
        piece.sides = n
        return vl

    def slab(self, piece, top, bot):
        """A closed slab between two grids of points (rows x columns): the top,
        the bottom and the walls round the edge."""
        bm = self.bm
        T = [[bm.verts.new(Vector(q)) for q in row] for row in top]
        B = [[bm.verts.new(Vector(q)) for q in row] for row in bot]
        for row in T + B:
            piece.verts += row
        na, nb = len(T), len(T[0])
        for i in range(na - 1):
            for j in range(nb - 1):
                piece.faces.append(bm.faces.new((T[i][j], T[i + 1][j], T[i + 1][j + 1], T[i][j + 1])))
                piece.faces.append(bm.faces.new((B[i][j], B[i][j + 1], B[i + 1][j + 1], B[i + 1][j])))
        edge = [(i, 0) for i in range(na - 1)] + [(na - 1, j) for j in range(nb - 1)] + \
               [(i, nb - 1) for i in range(na - 1, 0, -1)] + [(0, j) for j in range(nb - 1, 0, -1)]
        for k, (i, j) in enumerate(edge):
            i2, j2 = edge[(k + 1) % len(edge)]
            piece.faces.append(bm.faces.new((T[i][j], B[i][j], B[i2][j2], T[i2][j2])))

    def gadget(self, piece, S):
        """Finish a gadget part: rigid to its forearm."""
        self.finish(piece, self.rigid(f"forearm.{S}"))
        piece.gadget = f"forearm.{S}"

    def dash(self, name, c, along, nrm, weights, anchor, mat="Arms_Thread"):
        """One stitch dash on a surface point c with outward normal nrm, its
        length along `along`."""
        ln, wd, _, proud, sink = STITCH
        nrm = nrm.normalized()
        along = (along - nrm * along.dot(nrm)).normalized()
        across = nrm.cross(along).normalized()
        p = self.piece(name, mat)
        self.box(p, c + nrm * ((proud - sink) / 2), along, across, nrm, ln, wd, proud + sink)
        self.finish(p, weights)
        self.attach.append((name, anchor))
        self.stitches.append((name, along, across))
        return p

    def dash_row(self, prefix, a, b, nrm, weights, anchor, start=0, keep=None):
        """Stitch dashes along a straight run from a to b, centred in it, one
        dash and one gap apart, leaving out any whose centre `keep` rejects.
        Returns the number laid."""
        ln, _, gap, _, _ = STITCH
        L = (b - a).length
        count = int((L + gap) // (ln + gap))
        if count <= 0:
            return 0
        d = (b - a).normalized()
        used = count * ln + (count - 1) * gap
        s0 = (L - used) / 2 + ln / 2
        laid = 0
        for k in range(count):
            c = a + d * (s0 + k * (ln + gap))
            if keep is None or keep(c):
                self.dash(f"{prefix}.{start + laid + 1}", c, d, nrm, weights, anchor)
                laid += 1
        return laid

    # ---- weights ------------------------------------------------------------

    def arm_weights(self, sd):
        S = sd.tag

        def ws(p):
            tu = (p - sd.S).dot(sd.au)
            w_sh = 1.0 - smoothstep(-0.03, 0.03, tu)
            w_f = smoothstep(-0.02, 0.02, sd.s_elbow(p))    # 4 cm across the elbow
            w_h = smoothstep(-0.03, 0.01, sd.s_wrist(p))    # 4 cm across the wrist
            return [(f"shoulder.{S}", w_sh * (1 - w_f)), (f"upper_arm.{S}", (1 - w_sh) * (1 - w_f)),
                    (f"forearm.{S}", w_f * (1 - w_h)), (f"hand.{S}", w_f * w_h)]
        return ws

    def wrist_weights(self, sd):
        S = sd.tag

        def ws(p):
            w_h = smoothstep(-0.03, 0.01, sd.s_wrist(p))
            return [(f"forearm.{S}", 1 - w_h), (f"hand.{S}", w_h)]
        return ws

    def cuff_weights(self, sd):
        """The cuff surface's own weights: the wrist blend taken at its rings
        and interpolated between them, as the skinned facets are. Detail on
        the cuff takes these, so it moves with the facet under it."""
        ws = self.wrist_weights(sd)
        rings = [(s, dict(ws(sd.on_forearm(s)))) for s, _ in CUFF]

        def f(p):
            s = min(max(sd.s_wrist(p), rings[0][0]), rings[-1][0])
            for (sa, wa), (sb, wb) in zip(rings, rings[1:]):
                if sa <= s <= sb:
                    t = (s - sa) / (sb - sa)
                    return [(n, wa[n] * (1 - t) + wb[n] * t) for n in wa]
        return f

    @staticmethod
    def rigid(name):
        return lambda p: [(name, 1.0)]

    # ---- the arms -----------------------------------------------------------

    def _ring(self, c, axis, r, ref, tilt=None):
        u, w = ring_frame(axis, ref)
        R = Matrix.Identity(3)
        if tilt:
            ang, deg = tilt
            R = Matrix.Rotation(math.radians(deg), 3, u * math.cos(ang) + w * math.sin(ang))
        out = []
        for k in range(SIDES):
            a = 2 * math.pi * (k + 0.5) / SIDES
            out.append(c + R @ ((u * math.cos(a) + w * math.sin(a)) * r))
        return out

    def arms(self):
        """Each arm tube, capped at the shoulder and inside the glove at the
        wrist, with the forearm's stepped cloth bands."""
        for S, seed in (("L", 7), ("R", 19)):
            sd = self.sides[S]
            rng = random.Random(seed)
            ref = X
            loops = []
            loops.append(self._ring(sd.S - sd.au * 0.035, sd.au, 0.044, ref))      # shoulder cap
            loops.append(self._ring(sd.S, sd.au, 0.054, ref))
            loops.append(self._ring(sd.S + sd.au * 0.13, sd.au, UPPER_R, ref))
            loops.append(self._ring(sd.E - sd.au * 0.045, sd.au, 0.047, ref))
            loops.append(self._ring(sd.E, (sd.au + sd.af).normalized(), 0.049, ref))
            loops.append(self._ring(sd.on_forearm(-FORE_LEN + 0.025), sd.af, sd.fore_r(-FORE_LEN + 0.025) + 0.001, ref))
            # Bands, elbow to wrist: band k runs from its foot at b[k] (wrist
            # side) up to its lip just short of b[k+1] (elbow side), where a
            # 60 degree bevel drops to the next band's foot.
            b = [BAND_START]
            for _ in range(BAND_COUNT):
                b.append(b[-1] - rng.uniform(*BAND_W))
            hs = [rng.uniform(*BAND_H) for _ in range(BAND_COUNT)]
            tilts = [(rng.uniform(0, 2 * math.pi), rng.uniform(*BAND_TILT) * rng.choice((-1, 1)))
                     for _ in range(BAND_COUNT + 1)]
            band_loops = []   # the band edges that show: behind the cuff's rear face
            foot_at, lip_at = {}, {}
            for k in range(BAND_COUNT, -1, -1):
                foot = self._ring(sd.on_forearm(b[k]), sd.af, sd.fore_r(b[k]), ref, tilts[k])
                foot_at[k] = len(loops)
                loops.append(foot)
                if b[k] < CUFF[0][0]:
                    band_loops.append(foot)
                if k > 0:
                    # Band k-1's lip stands over this foot, a bevel's run toward the wrist.
                    h = hs[k - 1]
                    sl = b[k] + h / math.tan(BEVEL)
                    lip = self._ring(sd.on_forearm(sl), sd.af, sd.fore_r(sl) + h, ref, tilts[k])
                    lip_at[k - 1] = len(loops)
                    loops.append(lip)
                    if sl < CUFF[0][0]:
                        band_loops.append(lip)
            loops.append(self._ring(sd.on_forearm(0.002), sd.af, sd.fore_r(0.0) - 0.001, ref))   # inside the glove
            sd.bands = (b, hs)
            p = self.piece(f"{S}.arm", "Arms_Wrap")
            vl = self.loft(p, loops)
            self.finish(p, self.arm_weights(sd))
            p.sides = SIDES
            # Grime bands: band k is the bevel up from foot k+1 to its lip and
            # the slope down from the lip to foot k (loft pairs foot k+1 and lip k).
            for k in GRIME_BANDS:
                for pair in (foot_at[k + 1], lip_at[k]):
                    for f in p.faces[pair * SIDES:(pair + 1) * SIDES]:
                        f.material_index = M["Arms_Grime"]
            self.band_edges.append((S, [vl[i] for i in range(len(loops)) if any(loops[i] is bl for bl in band_loops)], vl))
            self.covered.append((p.name, vl[-1], [f"{S}.cuff", f"{S}.palm"]))
            self.refresh()

    def gloves(self):
        """The flared cuffs over the wraps and the faceted hands."""
        for S in "LR":
            sd = self.sides[S]
            loops = [self._ring(sd.on_forearm(s), sd.af, r, X) for s, r in CUFF]
            self.cuff_loops[S] = loops
            p = self.piece(f"{S}.cuff", "Arms_Glove")
            self.loft(p, loops)
            self.finish(p, self.wrist_weights(sd))
            p.sides = SIDES
            self.attach.append((p.name, f"{S}.arm"))
            # The hand: rings across it, the knuckle line slanted.
            loops = []
            for i, (s, hw, hd, hv) in enumerate(PALM):
                k = 1.0 if i == 0 else GLOVE_BULK   # the wrist ring tapers into the cuff
                hw, hd, hv = hw * k, hd * k, hv * k
                loop = []
                for tx, ny in PALM_SHAPE:
                    lat = tx * hw
                    h = ny * (hd if ny > 0 else hv)
                    reach = palm_front(lat) if s is None else s
                    loop.append(sd.W + sd.f * reach + sd.t * lat + sd.n * h)
                loops.append(loop)
            self.palm_loops[S] = loops
            p = self.piece(f"{S}.palm", "Arms_Glove")
            vl = self.loft(p, loops)
            self.finish(p, self.wrist_weights(sd))
            self.attach.append((p.name, f"{S}.cuff"))
            self.covered.append((p.name, vl[0], [f"{S}.cuff"]))
        self.refresh()

    def _segments(self, S, digit, pts, frames, widths, thicks, anchor):
        n = len(frames)
        for i, ((d, up), w, th) in enumerate(zip(frames, widths, thicks)):
            a = pts[i] - d * OVERLAP
            last = i == n - 1
            b = pts[i + 1] + (d * OVERLAP if not last else Vector())
            name = f"{S}.{digit}.{i + 1}"
            p = self.piece(name, "Arms_Glove")
            w1, t1 = w * 0.93, th * 0.93
            side = d.cross(up).normalized()
            up2 = side.cross(d).normalized()
            # A chamfered back end (the knuckle), full size 4 mm in; every
            # section a 12-sided cut box, so no crease ink runs along it.
            a2 = a + d * 0.004
            l0 = section_pts(a, side, up2, w * 0.8, th * 0.78)
            la = section_pts(a2, side, up2, w, th)
            mid = b - d * 0.006 if last else b
            if last:
                # A rounded fingertip: full size to 6 mm short of the tip, then smaller.
                lm = section_pts(mid, side, up2, w1, t1)
                lq = section_pts(b - d * 0.0025 - up2 * (t1 * 0.05), side, up2, w1 * 0.86, t1 * 0.80)
                lt = section_pts(b - up2 * (t1 * 0.12), side, up2, w1 * 0.62, t1 * 0.55)
                self.loft(p, [l0, la, lm, lq, lt])
            else:
                lb = section_pts(b, side, up2, w1, t1)
                self.loft(p, [l0, la, lb])
            p.sides = 12
            self.segs[(S, digit, i + 1)] = dict(joint=pts[i], end=pts[i + 1], a2=a2, b=b, mid=mid, d=d, up=up2,
                                                side=side, w=w, th=th, w1=w1, t1=t1, last=last, anchor=name)
            self.finish(p, self.rigid(f"{digit}.{i + 1}.{S}"))
            self.attach.append((name, anchor))
            anchor = name

    def fingers(self):
        """Four fingers and a thumb a hand, each a chain of three blocky
        segments overlapping at the knuckles."""
        for S in "LR":
            sd = self.sides[S]
            for name, lat, dors, splay, lens, curls, w0, t0 in FINGERS:
                pts, frames, hinge, w0, t0 = sd.fingers[name]
                widths = [w0, w0 * 0.92, w0 * 0.86]
                thicks = [t0, t0 * 0.92, t0 * 0.85]
                self._segments(S, name, pts, frames, widths, thicks, f"{S}.palm")
            pts, frames, _ = sd.thumb
            self._segments(S, "thumb", pts, frames, [w * GLOVE_BULK for w in THUMB_W],
                           [t * GLOVE_BULK for t in THUMB_T], f"{S}.palm")
        self.refresh()

    # ---- glove and wrap detail ---------------------------------------------

    @staticmethod
    def cuff_r(s):
        """The cuff's radius at s, between its rings."""
        for (sa, ra), (sb, rb) in zip(CUFF, CUFF[1:]):
            if sa <= s <= sb:
                return ra + (rb - ra) * (s - sa) / (sb - sa)
        raise ValueError(f"s {s} is off the cuff")

    def cuff_dirs(self, sd):
        """The 12 directions of the cuff's ring vertices, round the forearm."""
        c = sd.on_forearm(0.0)
        return [q - c for q in self._ring(c, sd.af, 1.0, X)]

    def glove_strap(self, sd, name, mat, ss, weights):
        """A strap round the cuff, following it from ss[0] to ss[-1]: sunk
        0.6 mm into it, standing STRAP_PROUD out of it."""
        sections = []
        for dvec in self.cuff_dirs(sd):
            inner = [sd.on_forearm(s) + dvec * (self.cuff_r(s) - 0.0006) for s in ss]
            outer = [sd.on_forearm(s) + dvec * (self.cuff_r(s) + STRAP_PROUD) for s in ss]
            sections.append([inner[0]] + outer + list(reversed(inner[1:])))
        p = self.piece(name, mat)
        self.ring_loft(p, sections)
        p.sides = SIDES
        self.finish(p, weights)
        return p

    def outer_angle(self, sd):
        """The angle round the forearm (from its back) whose radial points most outward (+X left, -X right)."""
        side = sd.af.cross(sd.back).normalized()
        sgn = 1 if sd.tag == "L" else -1
        return float(max(range(360), key=lambda a: sgn * (sd.back * math.cos(math.radians(a)) +
                                                          side * math.sin(math.radians(a))).x))

    def radial(self, sd, a):
        side = sd.af.cross(sd.back).normalized()
        r = math.radians(a)
        return (sd.back * math.cos(r) + side * math.sin(r)).normalized()

    def palm_surface(self, S, i, k):
        """A point on the palm's surface between its rings (i, 0..rings-1) and
        round them (k, wrapping), and the outward normal there."""
        loops = self.palm_loops[S]
        sd = self.sides[S]
        n = len(loops[0])

        def P(i, k):
            i0 = min(int(math.floor(i)), len(loops) - 2)
            fi = i - i0
            k0 = int(math.floor(k))
            fk = k - k0
            k0 %= n
            k1 = (k0 + 1) % n
            a = loops[i0][k0].lerp(loops[i0][k1], fk)
            c = loops[i0 + 1][k0].lerp(loops[i0 + 1][k1], fk)
            return a.lerp(c, fi)
        e = 0.02
        p = P(i, k)
        nrm = (P(min(i + e, len(loops) - 1), k) - P(max(i - e, 0.0), k)).cross(P(i, k + e) - P(i, k - e)).normalized()
        axis = sd.W + sd.f * (p - sd.W).dot(sd.f)
        if nrm.dot(p - axis) < 0:
            nrm = -nrm
        return p, nrm

    def palm_patch(self, S, name, i0, i1, k0, k1, steps=5):
        top, bot = [], []
        for a in range(steps):
            i = i0 + (i1 - i0) * a / (steps - 1)
            rt, rb = [], []
            for c in range(steps):
                k = k0 + (k1 - k0) * c / (steps - 1)
                p, nrm = self.palm_surface(S, i, k)
                rt.append(p + nrm * PATCH[0])
                rb.append(p - nrm * PATCH[1])
            top.append(rt)
            bot.append(rb)
        p = self.piece(name, "Arms_Worn")
        self.slab(p, top, bot)
        self.finish(p, self.wrist_weights(self.sides[S]))
        self.attach.append((name, f"{S}.palm"))

    def glove_detail(self):
        """Stitching along every finger and thumb and round the cuff's flare,
        knuckle pads, worn patches on the palm, the back of the hand and every
        fingertip, and a strap with a snap rivet round each cuff. Each piece
        takes the weights of the glove surface under it."""
        ln, wd, gap, proud, sink = STITCH
        for S in "LR":
            sd = self.sides[S]
            for digit in ("thumb", "index", "middle", "ring", "pinky"):
                for i in (1, 2, 3):
                    g = self.segs[(S, digit, i)]
                    d, side = g["d"], g["side"]
                    run = (g["mid"] - g["a2"]).dot(d)

                    def half_w(q, g=g, run=run):
                        f = (q - g["a2"]).dot(g["d"]) / run
                        return (g["w"] + (g["w1"] - g["w"]) * f) / 2
                    a = g["joint"] + d * 0.0015
                    b = (g["mid"] - d * 0.0045) if g["last"] else (g["end"] - d * 0.0015)
                    joint = f"{digit}.{i}.{S}"
                    for tag, sgn in (("a", 1), ("b", -1)):
                        A = a + side * (sgn * half_w(a))
                        B = b + side * (sgn * half_w(b))
                        self.dash_row(f"{S}.stitch_{digit}.{i}.{tag}", A, B, side * sgn, self.rigid(joint), g["anchor"])
                # A worn patch over the fingertip: the .3 segment's last 9 mm, grown 0.8 mm.
                g = self.segs[(S, digit, 3)]
                d, side, up = g["d"], g["side"], g["up"]
                p0 = g["mid"] - d * 0.003
                f = (p0 - g["a2"]).dot(d) / (g["mid"] - g["a2"]).dot(d)
                wp, tp = g["w"] + (g["w1"] - g["w"]) * f, g["th"] + (g["t1"] - g["th"]) * f
                w1, t1, bb = g["w1"], g["t1"], g["b"]
                gr = PATCH[0]
                loops = [section_pts(p0, side, up, wp, tp, -PATCH[1]), section_pts(p0, side, up, wp, tp, gr),
                         section_pts(g["mid"], side, up, w1, t1, gr),
                         section_pts(bb - d * 0.0025 - up * (t1 * 0.05), side, up, w1 * 0.86, t1 * 0.80, gr),
                         section_pts(bb - up * (t1 * 0.12) + d * gr, side, up, w1 * 0.62, t1 * 0.55, gr)]
                name = f"{S}.patch_tip_{digit}"
                p = self.piece(name, "Arms_Worn")
                self.loft(p, loops)
                p.sides = 12
                self.finish(p, self.rigid(f"{digit}.3.{S}"))
                self.attach.append((name, g["anchor"]))
            # Knuckle pads over each index-pinky .1 joint, bevelled.
            for digit in ("index", "middle", "ring", "pinky"):
                g = self.segs[(S, digit, 1)]
                d, side, up = g["d"], g["side"], g["up"]
                across, along, h, bev = PAD
                c = g["joint"] + d * (0.0005 + along / 2)
                f = (c - g["a2"]).dot(d) / (g["mid"] - g["a2"]).dot(d)
                c = c + up * ((g["th"] + (g["t1"] - g["th"]) * f) / 2)

                def octa(hx, hy, cut, z, c=c, side=side, d=d, up=up):
                    pts2 = [(hx - cut, -hy), (hx, -hy + cut), (hx, hy - cut), (hx - cut, hy),
                            (-hx + cut, hy), (-hx, hy - cut), (-hx, -hy + cut), (-hx + cut, -hy)]
                    return [c + side * x + d * y + up * z for x, y in pts2]
                name = f"{S}.pad_{digit}"
                p = self.piece(name, "Arms_Worn")
                self.loft(p, [octa(across / 2, along / 2, 0.0015, -0.0012), octa(across / 2, along / 2, 0.0015, h - bev),
                              octa(across / 2 - bev, along / 2 - bev, 0.001, h)])
                self.finish(p, self.rigid(f"{digit}.1.{S}"))
                self.attach.append((name, g["anchor"]))
            # Worn patches on the back of the hand and a reinforced palm patch.
            self.palm_patch(S, f"{S}.patch_back", 2.3, 4.4, 2.4, 4.6)
            self.palm_patch(S, f"{S}.patch_palm", 1.6, 4.4, 8.7, 10.6)
            # Stitching round the cuff's flare: dashes along each of its 12 facets.
            P1, P2 = self.cuff_loops[S][1], self.cuff_loops[S][2]
            for j in range(SIDES):
                jj = (j + 1) % SIDES
                A, B = P1[j].lerp(P2[j], CUFF_SEAM), P1[jj].lerp(P2[jj], CUFF_SEAM)
                nrm = (B - A).cross(P2[j] - P1[j])
                mid = (A + B) / 2
                if nrm.dot(mid - (sd.W + sd.af * (mid - sd.W).dot(sd.af))) < 0:
                    nrm = -nrm
                self.dash_row(f"{S}.stitch_cuff.{j + 1}", A, B, nrm, self.cuff_weights(sd), f"{S}.cuff")
            # A strap round the cuff, closed by a snap rivet on its outer side.
            name = f"{S}.cuffstrap"
            self.glove_strap(sd, name, "Arms_Strap", CUFF_STRAP, self.cuff_weights(sd))
            self.attach.append((name, f"{S}.cuff"))
            dirs = self.cuff_dirs(sd)
            sgn = 1 if S == "L" else -1
            j = max(range(SIDES), key=lambda j: sgn * (dirs[j] + dirs[(j + 1) % SIDES]).x)
            nrm = (dirs[j] + dirs[(j + 1) % SIDES]).normalized()
            sc = (CUFF_STRAP[0] + CUFF_STRAP[1]) / 2
            on = sd.on_forearm(sc) + (dirs[j] + dirs[(j + 1) % SIDES]) / 2 * (self.cuff_r(sc) + STRAP_PROUD)
            name = f"{S}.cuffstrap_rivet"
            p = self.piece(name, "Arms_Steel")
            self.cylinder(p, on - nrm * 0.0005, on + nrm * 0.0015, 0.003, 8, ref=sd.af, bevel=0.0003)
            self.cylinder(p, on + nrm * 0.0012, on + nrm * 0.0023, 0.0017, 8, ref=sd.af)
            self.finish(p, self.cuff_weights(sd))
            self.attach.append((name, f"{S}.cuffstrap"))
        self.refresh()

    def wrap_detail(self):
        """A frayed Arms_Grime tail from the band nearest the cuff, on each
        forearm's outer side: a knot and three strands of uneven length,
        rigid to the forearm. (The grime bands are coloured in arms().)"""
        for S in "LR":
            sd = self.sides[S]
            rad = self.radial(sd, self.outer_angle(sd))
            tang = sd.af.cross(rad).normalized()
            knot_c = sd.on_forearm(TAIL_S) + rad * (sd.fore_r(TAIL_S) + 0.003)
            name = f"{S}.tail_root"
            p = self.piece(name, "Arms_Grime")
            self.box(p, knot_c, sd.af, tang, rad, 0.010, 0.009, 0.006)
            self.finish(p, self.rigid(f"forearm.{S}"))
            p.gadget = f"forearm.{S}"
            self.attach.append((name, f"{S}.arm"))
            for j, L in enumerate(TAIL_LEN):
                start = knot_c + sd.af * (0.004 * (j - 1)) + rad * 0.001
                D0 = (rad * 0.55 - Z * 0.8 + sd.af * (0.15 * (j - 1))).normalized()
                pts, N = [start], 6
                for k in range(1, N + 1):
                    D = D0.slerp(-Z, 0.35 * k / N)
                    pts.append(pts[-1] + D * (L / N))
                loops = []
                for k, c in enumerate(pts):
                    D = (pts[min(k + 1, N)] - pts[max(k - 1, 0)]).normalized()
                    wdir = (sd.af - D * sd.af.dot(D)).normalized()
                    tdir = D.cross(wdir).normalized()
                    ww = TAIL_W * (1.0 - 0.25 * k / N)
                    loop = [c + wdir * (x * ww / 2) + tdir * (y * TAIL_T / 2) for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
                    if k == N:
                        # A ragged end: one edge cut back further than the other.
                        cut = 0.004 * (1 + j % 2)
                        loop = [q - D * cut if i in (1, 2) else q for i, q in enumerate(loop)]
                    loops.append(loop)
                name = f"{S}.tail.{j + 1}"
                p = self.piece(name, "Arms_Grime")
                self.loft(p, loops)
                self.finish(p, self.rigid(f"forearm.{S}"))
                p.gadget = f"forearm.{S}"
                self.attach.append((name, f"{S}.tail_root"))
        self.refresh()

    # ---- bracer and gauge (left forearm) -----------------------------------

    def _bracer_point(self, sd, s, ang, r):
        """A point on the left forearm at s, `ang` degrees round from its back, radius r."""
        side = sd.af.cross(sd.back).normalized()
        a = math.radians(ang)
        return sd.on_forearm(s) + (sd.back * math.cos(a) + side * math.sin(a)) * r

    def outside_angle(self, sd):
        """The angle round the left forearm (from its back) facing most outward, +X, off the plate."""
        side = sd.af.cross(sd.back).normalized()
        best = max((a for a in range(int(BRACER_SPAN) + 15, 360 - int(BRACER_SPAN) - 15)),
                   key=lambda a: (sd.back * math.cos(math.radians(a)) + side * math.sin(math.radians(a))).x)
        return float(best)

    def radius_at(self, sd, p):
        q = p - sd.W
        return (q - sd.af * q.dot(sd.af)).length

    def bracer_r(self, sd, s):
        return sd.fore_r(s) + BRACER_SIT

    def plate_point(self, sd, s, a, lift=0.0):
        """The point on the plate's outer surface at s, `a` degrees round
        (|a| <= BRACER_SPAN), raised `lift` along its facet's normal; and that normal."""
        ro = self.bracer_r(sd, s) + BRACER_T
        f = min(3, max(0, int((a + BRACER_SPAN) // 35.0)))
        c = -BRACER_SPAN + 35.0 * f + 17.5
        r = ro * math.cos(math.radians(17.5)) / math.cos(math.radians(a - c))
        nrm = self.radial(sd, c)
        return self._bracer_point(sd, s, a, r) + nrm * lift, nrm

    def angle_at(self, sd, q):
        """The angle round the forearm (from its back) of a point, and its s."""
        s = (q - sd.W).dot(sd.af)
        r = q - sd.on_forearm(s)
        side = sd.af.cross(sd.back).normalized()
        return math.degrees(math.atan2(r.dot(side), r.dot(sd.back))), s

    def bracer(self):
        """A brass plate curved over the back of the left forearm in four flat
        facets, its edges bevelled, held by two straps with stitched edges,
        each with a steel buckle frame and tongue and a keeper loop."""
        sd = self.sides["L"]
        angs = [-BRACER_SPAN + 2 * BRACER_SPAN * i / 4 for i in range(5)]
        s0, s1 = BRACER_S
        p = self.piece("L.bracer", "Arms_Brass")

        # Closed slab: a loop of outer then inner points at each section. The
        # long edges are bevelled on the facets' own planes, the ends by
        # lowering the end sections' outer points.
        def section(s, drop):
            ri = self.bracer_r(sd, s)
            ro = ri + BRACER_T - drop

            def P(a, r):
                return self._bracer_point(sd, s, a, r)
            outer = [P(angs[0], ro - PLATE_BEVEL), P(angs[0], ro).lerp(P(angs[1], ro), 0.08)]
            outer += [P(a, ro) for a in angs[1:4]]
            outer += [P(angs[4], ro).lerp(P(angs[3], ro), 0.08), P(angs[4], ro - PLATE_BEVEL)]
            inner = [P(a, ri) for a in reversed(angs)]
            return outer + inner
        self.loft(p, [section(s0, PLATE_BEVEL), section(s0 + PLATE_BEVEL, 0.0),
                      section(s1 - PLATE_BEVEL, 0.0), section(s1, PLATE_BEVEL)])
        self.gadget(p, "L")
        self.attach.append(("L.bracer", "L.arm"))
        # Straps: rings round the forearm, over the plate on the back.
        under = [BRACER_SPAN + (360 - 2 * BRACER_SPAN) * i / 5 for i in range(1, 5)]
        strap_angs = angs + under
        for k, sc in enumerate(STRAPS):
            p = self.piece(f"L.strap.{k + 1}", "Arms_Strap")
            # Cross-section loop per angle: (front inner, front outer, back outer, back inner).
            sections = []
            for a in strap_angs:
                on_plate = abs(a) <= BRACER_SPAN + 1e-6
                sec = []
                for s in (sc - STRAP_W / 2, sc + STRAP_W / 2):
                    if on_plate:
                        rin = self.bracer_r(sd, s) + BRACER_T - 0.001
                    else:
                        rin = sd.fore_r(s) + 0.0025
                    sec.append((s, rin))
                (sa, ra), (sb, rb) = sec
                sections.append([self._bracer_point(sd, sa, a, ra), self._bracer_point(sd, sa, a, ra + STRAP_T + 0.001),
                                 self._bracer_point(sd, sb, a, rb + STRAP_T + 0.001), self._bracer_point(sd, sb, a, rb)])
            bm = self.bm
            vs = [[bm.verts.new(q) for q in sec] for sec in sections]
            for sec in vs:
                p.verts += sec
            n = len(vs)
            for i in range(n):
                A, B = vs[i], vs[(i + 1) % n]
                for j in range(4):
                    jj = (j + 1) % 4
                    p.faces.append(bm.faces.new((A[j], A[jj], B[jj], B[j])))
            self.gadget(p, "L")
            self.attach.append((p.name, "L.arm"))
            self.attach.append((p.name, "L.bracer"))
            strap = p.name
            # The buckle, on the strap's outer surface on the outside of the
            # forearm: angles round the strap from the plate's edge, wrapping.
            wrapped = [a if a >= -BRACER_SPAN else a + 360 for a in strap_angs] + [strap_angs[0] + 360]
            mids = [(sec[1] + sec[2]) / 2 for sec in sections]
            a = self.outside_angle(sd)
            a = a if a >= -BRACER_SPAN else a + 360
            i = max(i for i in range(n) if wrapped[i] <= a)
            t = (a - wrapped[i]) / (wrapped[i + 1] - wrapped[i])
            ma, mb = mids[i], mids[(i + 1) % n]
            c = ma.lerp(mb, t)
            along = (mb - ma).normalized()
            across = sections[i][2] - sections[i][1]
            across = (across - along * across.dot(along)).normalized()
            out = along.cross(across).normalized()
            if out.dot(c - sd.on_forearm(sc)) < 0:
                out = -out
            q = self.piece(f"L.buckle_frame.{k + 1}", "Arms_Steel")
            self.frame(q, c + out * 0.0006, along, across, out, BUCKLE[0], STRAP_W + 0.003, 0.0022, 0.0016)
            self.gadget(q, "L")
            self.attach.append((q.name, strap))
            q = self.piece(f"L.buckle_tongue.{k + 1}", "Arms_Steel")
            self.box(q, c + out * 0.0007 - along * 0.002, along, across, out, 0.009, 0.0018, 0.0012)
            self.gadget(q, "L")
            self.attach.append((q.name, f"L.buckle_frame.{k + 1}"))
            # The keeper loop round the strap beside the buckle, gripping it.
            kc = c + along * 0.012 - out * ((STRAP_T + 0.001) / 2)
            q = self.piece(f"L.buckle_keeper.{k + 1}", "Arms_Strap")
            self.frame(q, kc, across, out, along, STRAP_W - 0.0004 + 0.0028, STRAP_T + 0.001 - 0.0004 + 0.0028,
                       0.0014, 0.004)
            self.gadget(q, "L")
            self.attach.append((q.name, strap))
            # Stitched edges: a row of dashes 2.5 mm in from each edge of the
            # strap's outer face, clear of the buckle and keeper.
            f_in = (STITCH[1] / 2 + 0.0012) / STRAP_W
            for tag, f in (("a", f_in), ("b", 1 - f_in)):
                count = 0
                for i in range(n):
                    A = sections[i][1].lerp(sections[i][2], f)
                    B = sections[(i + 1) % n][1].lerp(sections[(i + 1) % n][2], f)
                    nrm = (B - A).cross(sections[i][2] - sections[i][1])
                    if nrm.dot((A + B) / 2 - sd.on_forearm(sc)) < 0:
                        nrm = -nrm
                    count += self.dash_row(f"L.stitch_strap.{k + 1}.{tag}", A, B, nrm, self.rigid("forearm.L"),
                                           strap, start=count, keep=lambda q, c=c: (q - c).length > 0.024)
            for name in self.pieces:
                if name.startswith(f"L.stitch_strap.{k + 1}."):
                    self.pieces[name].gadget = "forearm.L"
        self.refresh()

    def gauge(self):
        """A brass bezel on the plate, turned toward the eye, its window
        recessed 3 mm, with a backlit screen reading 47% and a block cursor."""
        sd = self.sides["L"]
        ri = self.bracer_r(sd, GAUGE_S) + BRACER_T
        base = self._bracer_point(sd, GAUGE_S, 0.0, ri)
        plate_n = sd.back
        to_eye = (-base).normalized()
        n = plate_n.slerp(to_eye, GAUGE_TURN).normalized()
        # The window's long side lies level across the view, so the readout
        # runs across the screen; its short side runs up the forearm.
        w = (X - n * X.dot(n)).normalized()
        u = n.cross(w).normalized()
        if u.dot(sd.af) < 0:
            u, w = -u, -w
        hl, hw = WINDOW[1] / 2 + BEZEL, WINDOW[0] / 2 + BEZEL
        il, iw = WINDOW[1] / 2, WINDOW[0] / 2
        # Lift the bezel's face along n until its corners and the window's
        # floor clear the plate by 1.5 mm.
        def clear(lam):
            top = base + n * lam
            pts = [top + u * x + w * y for x in (-hl, hl) for y in (-hw, hw)]
            pts += [top - n * RECESS + u * x + w * y for x in (-il, il) for y in (-iw, iw)]
            return all(self.radius_at(sd, q) >= ri + 0.0015 for q in pts)
        lam = 0.0
        while not clear(lam):
            lam += 0.0002
            if lam > 0.03:
                raise RuntimeError("the gauge's bezel can't clear the plate within 3 cm")
        top = base + n * lam
        self.gauge_lift = lam
        bm = self.bm
        p = self.piece("L.gauge", "Arms_Brass")

        def tp(x, y, h=0.0):
            return top + u * x + w * y + n * h

        def on_plate(x, y):
            # Straight in toward the forearm's axis from the bezel's face, to
            # 4 mm inside the plate's outer surface.
            q = top + u * x + w * y
            rel = q - sd.W
            axial = sd.W + sd.af * rel.dot(sd.af)
            radial = q - axial
            return axial + radial.normalized() * (ri - 0.004)
        outer = [(-hl, -hw), (hl, -hw), (hl, hw), (-hl, hw)]
        inner = [(-il, -iw), (il, -iw), (il, iw), (-il, iw)]
        O = [bm.verts.new(tp(x, y)) for x, y in outer]
        I = [bm.verts.new(tp(x, y)) for x, y in inner]
        F = [bm.verts.new(tp(x, y, -RECESS)) for x, y in inner]
        B = [bm.verts.new(on_plate(x, y)) for x, y in outer]
        p.verts += O + I + F + B
        for i in range(4):
            j = (i + 1) % 4
            p.faces.append(bm.faces.new((O[i], O[j], I[j], I[i])))   # bezel face
            p.faces.append(bm.faces.new((I[i], I[j], F[j], F[i])))   # window walls
            p.faces.append(bm.faces.new((B[i], B[j], O[j], O[i])))   # sides
        p.faces.append(bm.faces.new(F))                              # window floor
        p.faces.append(bm.faces.new(list(reversed(B))))              # underside, in the plate
        self.gadget(p, "L")
        self.attach.append(("L.gauge", "L.bracer"))
        self.gauge_top = top
        self.gauge_frame = (top, u, w, n, hl, hw)
        self.gauge_on_plate = on_plate
        # The screen: a backlit panel filling the window floor, and on it
        # the readout's lit dots.
        floor = top - n * RECESS
        pw, pu, pt = LCD_PANEL
        q = self.piece("L.screen", "Arms_Screen")
        la, lb = self.box_loops(floor - n * pt, floor + n * pt, n, u, pw, pu)   # pw along w, pu along u
        self.loft(q, [la, lb])
        self.gadget(q, "L")
        self.attach.append(("L.screen", "L.gauge"))
        # Reading directions on screen: `right` the one of +-w whose end
        # projects further right (NDC x), `up` the one of +-u that projects higher.
        right = w if project(floor + w * 0.01)[0] > project(floor)[0] else -w
        up = u if project(floor + u * 0.01)[1] > project(floor)[1] else -u
        face = floor + n * pt
        cols = 4 * len(LCD_TEXT) - 1
        for g, rows in enumerate(LCD_TEXT):
            for r, row in enumerate(rows):
                for c, lit in enumerate(row):
                    if lit != "X":
                        continue
                    centre = face + right * ((4 * g + c - (cols - 1) / 2) * LCD_DOT) + up * ((2 - r) * LCD_DOT)
                    name = f"L.lcd.{g + 1}.{r + 1}{c + 1}"
                    q = self.piece(name, "Arms_Glow")
                    la, lb = self.box_loops(centre - n * 0.0004, centre + n * LCD_PROUD, n, up, LCD_DOT, LCD_DOT)
                    self.loft(q, [la, lb])
                    self.gadget(q, "L")
                    self.attach.append((name, "L.screen"))
                    self.lcd.append((name, g, r, centre + n * LCD_PROUD, right, up, n))
        self.refresh()

    def bracer_detail(self):
        """The hygrometer's devices round the gauge, all rigid to forearm.L:
        four slotted screws on the bezel's corners, a knurled dial on its inner
        side, three toggles at the plate's wrist end, a copper coil on a brass
        tube behind the bezel, a copper and a rubber cable back under the
        straps into the wraps, and a canister clipped beside the plate."""
        sd = self.sides["L"]
        top, u, w, n, hl, hw = self.gauge_frame
        # Screws: an 8-sided head at each corner of the bezel, its slot cut
        # across by leaving a gap between the two halves of its top. The two
        # at the wrist end sit on the bezel's face; the two at the elbow end
        # on its elbow-side wall, by the corners, where the bottom edge of the
        # view would otherwise cut a sliver of each head.
        r = SCREW[0] / 2
        slot = (math.cos(math.radians(45)), math.sin(math.radians(45)))
        octagon = [(r * math.cos(2 * math.pi * (k + 0.5) / 8), r * math.sin(2 * math.pi * (k + 0.5) / 8)) for k in range(8)]
        for k, (sx, sy) in enumerate(((1, 1), (1, -1), (-1, 1), (-1, -1))):
            y = sy * (hw - SCREW_INSET)
            if sx > 0:
                c, a1, a2, ax = top + u * (hl - SCREW_INSET) + w * y, u, w, n
            else:
                o = top - u * hl + w * y
                down = (self.gauge_on_plate(-hl, y) - o).normalized()
                c = o + down * (SCREW_INSET + 0.0003)
                ax = w.cross(down).normalized()
                if ax.dot(u) > 0:
                    ax = -ax
                a1, a2 = down, w
            name = f"L.screw.{k + 1}"
            p = self.piece(name, "Arms_Steel")
            self.cylinder(p, c - ax * 0.0004, c + ax * 0.0008, r, 8, ref=a1)
            for sgn in (1, -1):
                half = clip_half(octagon, (sgn * slot[0], sgn * slot[1]), SCREW[1] / 2)
                self.loft(p, [[c + a1 * x + a2 * y + ax * h for x, y in half] for h in (0.0007, 0.0015)])
            self.gadget(p, "L")
            self.attach.append((name, "L.gauge"))
            self.screws.append(name)
        # The knurled dial, beside the bezel's inner side, its face turned toward the eye.
        Ro, Ri, ridges, th = KNURL
        D = top + w * (hw + 0.0085) - n * 0.002
        ad = w.slerp((-D).normalized(), KNURL_TURN).normalized()
        m1 = (u - ad * u.dot(ad)).normalized()
        m2 = ad.cross(m1).normalized()
        prof = []
        for j in range(ridges):
            phi = 360.0 * j / ridges
            prof += [(phi - 6.5, Ri), (phi - 4.0, Ro), (phi + 4.0, Ro), (phi + 6.5, Ri)]

        def ring(c, rr=None):
            return [c + (m1 * math.cos(math.radians(a)) + m2 * math.sin(math.radians(a))) * (R if rr is None else rr)
                    for a, R in prof]
        back, front = D - ad * (th / 2), D + ad * (th / 2)
        name = "L.knurl_dial"
        p = self.piece(name, "Arms_Steel")
        self.loft(p, [ring(back, 0.0045), ring(back), ring(front), ring(front, 0.0045)])
        self.cylinder(p, top + w * (hw - 0.0015) - n * 0.0025, back + ad * 0.0005, 0.0022, 8, ref=u)
        p.sides = len(prof)
        self.gadget(p, "L")
        self.attach.append((name, "L.gauge"))
        self.knurl = [front + (m1 * math.cos(math.radians(360.0 * j / ridges)) +
                               m2 * math.sin(math.radians(360.0 * j / ridges))) * (Ro * math.cos(math.radians(4.0)))
                      for j in range(ridges)]
        self.knurl_axis = (D, ad)
        name = "L.knurl_mark"
        p = self.piece(name, "Arms_Paint")
        self.box(p, front + m1 * 0.0033 + ad * 0.0002, m1, m2, ad, 0.0046, 0.0026, 0.0008)
        self.gadget(p, "L")
        self.attach.append((name, "L.knurl_dial"))
        # Toggles: a row of three just past the bezel's wrist edge, on the plate.
        for k, y in enumerate(TOGGLE_W):
            q = top + u * (hl + 0.0030) + w * y
            a, s = self.angle_at(sd, q)
            pt, nrm = self.plate_point(sd, s, a)
            names = (f"L.toggle_base.{k + 1}", f"L.toggle_lever.{k + 1}", f"L.toggle_tip.{k + 1}")
            p = self.piece(names[0], "Arms_Steel")
            self.box(p, pt + nrm * 0.0002, sd.af, sd.af.cross(nrm), nrm, 0.0042, 0.0042, 0.002)
            self.gadget(p, "L")
            self.attach.append((names[0], "L.bracer"))
            ld = nrm.slerp(sd.af, 0.3).normalized()
            l0 = pt + nrm * 0.0008
            l1 = l0 + ld * 0.0062
            p = self.piece(names[1], "Arms_Rubber")
            self.cylinder(p, l0, l1, 0.0011, 6, ref=sd.af, rb=0.0008)
            self.gadget(p, "L")
            self.attach.append((names[1], names[0]))
            p = self.piece(names[2], "Arms_Paint")
            self.cylinder(p, l1 - ld * 0.0006, l1 + ld * 0.0026, TOGGLE_TIP / 2, 8, ref=sd.af, bevel=0.0004)
            self.gadget(p, "L")
            self.attach.append((names[2], names[1]))
            self.toggles.append(names)
        # The coil: a short brass tube on the plate behind the bezel, and a
        # copper wire wound round it.
        c0, c1, turns, ang = COIL
        A, nrm = self.plate_point(sd, c0 - 0.003, ang, 0.0028)
        B, _ = self.plate_point(sd, c1 + 0.003, ang, 0.0028)
        p = self.piece("L.coil_tube", "Arms_Brass")
        self.cylinder(p, A, B, 0.0035, 12, ref=nrm, bevel=0.0006)
        self.gadget(p, "L")
        self.attach.append(("L.coil_tube", "L.bracer"))
        ax = (B - A).normalized()
        e1 = (nrm - ax * nrm.dot(ax)).normalized()
        e2 = ax.cross(e1)
        steps = turns * 12
        length = (B - A).length - 0.006
        path = [A + ax * (0.003 + length * i / steps) +
                (e1 * math.cos(2 * math.pi * i / 12) + e2 * math.sin(2 * math.pi * i / 12)) * 0.0042 for i in range(steps + 1)]
        p = self.piece("L.coil", "Arms_Copper")
        self.tube(p, path, 0.0009, 6)
        self.gadget(p, "L")
        self.attach.append(("L.coil", "L.coil_tube"))
        # Cables: out of the bezel's elbow-side wall, along the plate under the
        # straps, and down under the wrap band past the plate's elbow end.
        for tag, mat, y, rc in CABLES:
            P0 = top + u * (-hl + 0.0025) + w * y - n * 0.003
            a, s = self.angle_at(sd, top + u * (-hl - 0.002) + w * y)
            path = [P0]
            while s > BRACER_S[0] + 0.004:
                path.append(self.plate_point(sd, s, a, rc - 0.0003)[0])
                s -= 0.006
            path.append(self.plate_point(sd, BRACER_S[0], a, rc - 0.0006)[0] + sd.af * (-0.002))
            for s, dr in ((-0.168, 0.004), (-0.174, 0.0005), (CABLE_END, -0.004)):
                path.append(self._bracer_point(sd, s, a, sd.fore_r(s) + dr))
            name = f"L.cable_{tag}"
            p = self.piece(name, mat)
            vl = self.tube(p, path, rc, 6)
            self.gadget(p, "L")
            self.attach.append((name, "L.gauge"))
            self.covered.append((name, vl[0], ["L.gauge"]))
            self.covered.append((name, vl[-1], ["L.arm"]))
        # The canister, beside the plate's outer edge over the first strap,
        # held by a clip to the plate, a valve wheel on its wrist end.
        sE, sW, rc, ac = CANISTER
        Ee = self._bracer_point(sd, sE, ac, sd.fore_r(sE) + 0.012 + rc)
        Ew = self._bracer_point(sd, sW, ac, sd.fore_r(sW) + 0.012 + rc)
        ax = (Ew - Ee).normalized()
        p = self.piece("L.canister", "Arms_Steel")
        self.cylinder(p, Ee, Ew, rc, 12, ref=sd.back, bevel=0.0015)
        self.gadget(p, "L")
        m1 = (sd.back - ax * sd.back.dot(ax)).normalized()
        m2 = ax.cross(m1)
        p = self.piece("L.canister_valve", "Arms_Steel")
        self.cylinder(p, Ew - ax * 0.0005, Ew + ax * 0.004, 0.002, 8, ref=m1)
        self.cylinder(p, Ew + ax * 0.0035, Ew + ax * 0.0065, 0.0026, 8, ref=m1)
        Cv, Rv = Ew + ax * 0.0052, 0.0068
        secs = []
        for j in range(12):
            rd = m1 * math.cos(2 * math.pi * j / 12) + m2 * math.sin(2 * math.pi * j / 12)
            secs.append([Cv + rd * (Rv + x * 0.0007) + ax * (y * 0.0007) for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
        self.ring_loft(p, secs)
        for j in range(4):
            phi = math.radians(20 + 90 * j)
            rd = m1 * math.cos(phi) + m2 * math.sin(phi)
            self.box(p, Cv + rd * 0.0046, rd, ax.cross(rd), ax, 0.0050, 0.0014, 0.0012)
        self.gadget(p, "L")
        self.attach.append(("L.canister_valve", "L.canister"))
        Cc = Ee.lerp(Ew, 0.45)
        p = self.piece("L.canister_clip", "Arms_Steel")
        self.ring_loft(p, [circle(Cc - ax * 0.0025, ax, rc - 0.0003, 12, m1), circle(Cc - ax * 0.0025, ax, rc + 0.0013, 12, m1),
                           circle(Cc + ax * 0.0025, ax, rc + 0.0013, 12, m1), circle(Cc + ax * 0.0025, ax, rc - 0.0003, 12, m1)])
        _, s_c = self.angle_at(sd, Cc)
        Pp, _ = self.plate_point(sd, s_c, -BRACER_SPAN + 8.0, -0.001)
        foot = Cc + (Pp - Cc).normalized() * (rc + 0.0008)
        self.box(p, (foot + Pp) / 2, ax, ax.cross(Pp - foot), (Pp - foot).normalized(), 0.005, 0.002,
                 (Pp - foot).length + 0.001)
        self.gadget(p, "L")
        self.attach.append(("L.canister_clip", "L.bracer"))
        self.attach.append(("L.canister", "L.canister_clip"))
        self.refresh()

    def compass(self):
        """The sun compass on the right wrist, rigid to forearm.R: a strap
        over the middle of the cuff carrying a post, a round steel housing with
        its ivory face turned toward the eye, 12 tick marks, a fixed needle,
        an amber lamp on the face's rim, two side buttons, and a sun-shade lid
        hinged on the face's far edge (toward the fingers), propped open."""
        sd = self.sides["R"]
        s0, sm, s1 = COMPASS_STRAP

        def facing(a):
            p = sd.on_forearm(sm) + self.radial(sd, a) * 0.046
            return self.radial(sd, a).dot((-p).normalized())
        ac = float(max(range(-90, 91), key=facing))
        rad = self.radial(sd, ac)
        name = "R.compass_strap"
        p = self.glove_strap(sd, name, "Arms_Strap", COMPASS_STRAP, self.rigid("forearm.R"))
        p.gadget = "forearm.R"
        self.attach.append((name, "R.cuff"))
        r_out = self.cuff_r(sm) + STRAP_PROUD
        foot = sd.on_forearm(sm) + rad * (r_out * math.cos(math.radians(15)) - 0.0015)
        C0 = sd.on_forearm(sm) + rad * (r_out + COMPASS_POST)
        p = self.piece("R.compass_post", "Arms_Steel")
        self.cylinder(p, foot, C0 + rad * 0.001, 0.0035, 8, ref=sd.af)
        self.gadget(p, "R")
        self.attach.append(("R.compass_post", name))
        nF = rad.slerp((-(C0 + rad * 0.009)).normalized(), COMPASS_TURN).normalized()
        e1 = (sd.af - nF * sd.af.dot(nF)).normalized()     # toward the fingers
        e2 = nF.cross(e1)

        def fp(h, r, phi):
            a = math.radians(phi)
            return C0 + nF * h + (e1 * math.cos(a) + e2 * math.sin(a)) * r
        p = self.piece("R.compass_housing", "Arms_Steel")
        self.loft(p, [circle(C0, nF, 0.0155, 16, e1), circle(C0 + nF * 0.0015, nF, HOUSING_R, 16, e1),
                      circle(C0 + nF * 0.006, nF, HOUSING_R, 16, e1), circle(C0 + nF * 0.008, nF, 0.0145, 16, e1)])
        p.sides = 16
        self.gadget(p, "R")
        self.attach.append(("R.compass_housing", "R.compass_post"))
        # The face: a disc standing on the housing's narrowed top, so nothing
        # of the housing rises past it.
        p = self.piece("R.compass_face", "Arms_Dial")
        self.loft(p, [circle(C0 + nF * 0.0079, nF, FACE_R, 24, e1), circle(C0 + nF * 0.009, nF, FACE_R, 24, e1)])
        p.sides = 24
        self.gadget(p, "R")
        self.attach.append(("R.compass_face", "R.compass_housing"))
        face = ["R.compass_face"]
        for k in range(12):
            phi = 30.0 * k
            rd = fp(0, 1, phi) - C0
            name = f"R.compass_tick.{k + 1}"
            p = self.piece(name, "Arms_Paint")
            self.box(p, fp(0.0092, TICK[0], phi), rd, nF.cross(rd), nF, TICK[1], TICK[2], 0.0008)
            self.gadget(p, "R")
            self.attach.append((name, "R.compass_face"))
            face.append(name)
        tip_r, tail_r, nw, nphi = NEEDLE
        perp = fp(0, 1, nphi + 90) - C0
        pts2 = [fp(0, tip_r, nphi) - C0, perp * (nw / 2), fp(0, tail_r, nphi + 180) - C0, perp * (-nw / 2)]
        p = self.piece("R.compass_needle", "Arms_Paint")
        self.loft(p, [[C0 + nF * h + q for q in pts2] for h in (0.0088, 0.0096)])
        self.gadget(p, "R")
        self.attach.append(("R.compass_needle", "R.compass_face"))
        face.append("R.compass_needle")
        p = self.piece("R.compass_hub", "Arms_Steel")
        self.cylinder(p, C0 + nF * 0.0088, C0 + nF * 0.0103, 0.0022, 8, ref=e1, bevel=0.0003)
        self.gadget(p, "R")
        self.attach.append(("R.compass_hub", "R.compass_needle"))
        face.append("R.compass_hub")
        lr, lrad, lphi = LAMP
        Lc = fp(0, lr, lphi)
        p = self.piece("R.compass_lamp", "Arms_Amber")
        self.cylinder(p, Lc + nF * 0.0087, Lc + nF * 0.0102, lrad, 8, ref=e1, bevel=0.0004)
        self.gadget(p, "R")
        self.attach.append(("R.compass_lamp", "R.compass_face"))
        face.append("R.compass_lamp")
        for j, phi in enumerate((200.0, 235.0)):
            rd = fp(0, 1, phi) - C0
            base = C0 + nF * 0.0037 + rd * (HOUSING_R - 0.001)
            name = f"R.compass_button.{j + 1}"
            p = self.piece(name, "Arms_Rubber")
            self.cylinder(p, base, base + rd * 0.0035, 0.0021, 8, ref=nF, bevel=0.0004)
            self.gadget(p, "R")
            self.attach.append((name, "R.compass_housing"))
        # The lid: hinged on the face's far edge, swung open past upright.
        Hc = C0 + e1 * 0.0185 + nF * 0.0062
        p = self.piece("R.compass_hinge", "Arms_Steel")
        self.cylinder(p, Hc - e2 * 0.007, Hc + e2 * 0.007, 0.0016, 8, ref=e1)
        self.gadget(p, "R")
        self.attach.append(("R.compass_hinge", "R.compass_housing"))
        Rm = Matrix.Rotation(math.radians(LID_OPEN), 3, e2)

        def opened(q):
            return Hc + Rm @ (q - Hc)
        Lc0 = C0 + nF * 0.0108
        p = self.piece("R.compass_lid", "Arms_Steel")
        self.loft(p, [[opened(q) for q in circle(Lc0 + nF * dh, nF, 0.0175, 16, e1)] for dh in (-0.0007, 0.0007)])
        self.box(p, opened(C0 + e1 * 0.0175 + nF * 0.0086), Rm @ e1, Rm @ e2, Rm @ nF, 0.0035, 0.008, 0.0048)
        p.sides = 16
        self.gadget(p, "R")
        self.attach.append(("R.compass_lid", "R.compass_hinge"))
        Ph = fp(0.005, HOUSING_R - 0.001, 70.0)
        Pl = opened(C0 + nF * 0.0101 + e1 * 0.004 + e2 * 0.012)
        d = (Pl - Ph).normalized()
        p = self.piece("R.compass_prop", "Arms_Steel")
        self.box(p, (Ph + Pl) / 2, e1, d.cross(e1), d, 0.0013, 0.0013, (Pl - Ph).length + 0.002)
        self.gadget(p, "R")
        self.attach.append(("R.compass_prop", "R.compass_housing"))
        self.attach.append(("R.compass_prop", "R.compass_lid"))
        self.compass_info = dict(centre=C0 + nF * 0.009, normal=nF, face=face, dial="R.compass_face",
                                 lamp="R.compass_lamp", lid="R.compass_lid", angle=ac)
        self.refresh()

    # ---- rig ------------------------------------------------------------------

    def bone_table(self):
        """name -> (head, tail, parent, roll reference)."""
        t = {"root": (v(0, 0, 0), v(0, -0.1, 0), None, Z)}
        for S in "LR":
            sd = self.sides[S]
            t[f"shoulder.{S}"] = (sd.C, sd.S, "root", Z)
            t[f"upper_arm.{S}"] = (sd.S, sd.E, f"shoulder.{S}", sd.back)
            t[f"forearm.{S}"] = (sd.E, sd.W, f"upper_arm.{S}", sd.back)
            t[f"hand.{S}"] = (sd.W, sd.K_mid, f"forearm.{S}", sd.n)
            for name, *_ in FINGERS:
                pts, frames, *_ = sd.fingers[name]
                parent = f"hand.{S}"
                for i in range(3):
                    t[f"{name}.{i + 1}.{S}"] = (pts[i], pts[i + 1], parent, frames[i][1])
                    parent = f"{name}.{i + 1}.{S}"
            pts, frames, _ = sd.thumb
            parent = f"hand.{S}"
            for i in range(3):
                t[f"thumb.{i + 1}.{S}"] = (pts[i], pts[i + 1], parent, frames[i][1])
                parent = f"thumb.{i + 1}.{S}"
        return t

    def rig(self):
        arm = bpy.data.armatures.new(RIG)
        rig = bpy.data.objects.new(RIG, arm)
        self.coll.objects.link(rig)
        arm.display_type = "STICK"
        rig.show_in_front = True
        view_layer = bpy.context.view_layer
        prev_active = view_layer.objects.active
        for ob in view_layer.objects:
            if ob.select_get():
                ob.select_set(False)
        view_layer.objects.active = rig
        rig.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        table = self.bone_table()
        for name in JOINTS:
            head, tail, parent, ref = table[name]
            eb = arm.edit_bones.new(name)
            eb.head, eb.tail = head, tail
            eb.align_roll(ref)
            if parent:
                eb.parent = arm.edit_bones[parent]
                eb.use_connect = False
        bpy.ops.object.mode_set(mode="OBJECT")
        rig.select_set(False)
        if prev_active and prev_active.name in view_layer.objects:
            view_layer.objects.active = prev_active
        self.refresh()
        self.obj.parent = rig
        mod = self.obj.modifiers.new("Armature", "ARMATURE")
        mod.object = rig
        self.rig_obj = rig

    def build_all(self):
        for step in self.STEPS:
            getattr(self, step)()

    # ---- poses ----------------------------------------------------------------

    def clear_pose(self):
        for pb in self.rig_obj.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion()
            pb.location = Vector()
            pb.scale = Vector((1, 1, 1))

    def turn(self, name, world_axis, deg):
        """Turn a bone about a world-space axis (taken at rest), after its parent."""
        pb = self.rig_obj.pose.bones[name]
        local = self.rig_obj.data.bones[name].matrix_local.to_3x3().inverted() @ Vector(world_axis)
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion(local.normalized(), math.radians(deg)) @ pb.rotation_quaternion

    def test_poses(self):
        """name -> function setting the pose: the attachment check's test poses."""
        def wrist(kind, deg):
            def f():
                for S in "LR":
                    sd = self.sides[S]
                    ax = {"twist": sd.af, "flex": sd.t, "sideways": sd.n}[kind]
                    self.turn(f"hand.{S}", ax, deg)
            return f

        def curl():
            for S in "LR":
                sd = self.sides[S]
                for name, *_ in FINGERS:
                    hinge = sd.fingers[name][2]
                    for i in range(3):
                        self.turn(f"{name}.{i + 1}.{S}", hinge, 60.0)
                hinge = sd.thumb[2]
                for i in range(3):
                    self.turn(f"thumb.{i + 1}.{S}", hinge, 60.0)

        def elbow():
            for S in "LR":
                sd = self.sides[S]
                hinge = sd.au.cross(sd.af).normalized()
                self.turn(f"forearm.{S}", hinge, -30.0)   # opens the elbow (checked in check())

        poses = {"rest": lambda: None}
        for kind in ("twist", "flex", "sideways"):
            for deg in (30.0, -30.0):
                poses[f"wrist {kind} {deg:+.0f}"] = wrist(kind, deg)
        def elbow_closed():
            for S in "LR":
                sd = self.sides[S]
                hinge = sd.au.cross(sd.af).normalized()
                self.turn(f"forearm.{S}", hinge, ELBOW_CLOSE_TEST)   # closes the elbow (checked in check())

        poses["fingers curled 60"] = curl
        poses["elbows opened 30"] = elbow
        poses[f"elbows closed {ELBOW_CLOSE_TEST:.0f}"] = elbow_closed
        return poses

    # ---- clips ----------------------------------------------------------------

    def rest_np(self):
        """Each joint's rest matrix (armature space, the world here), as float64."""
        bones = self.rig_obj.data.bones
        rest = {n: np.array(bones[n].matrix_local, dtype=np.float64) for n in JOINTS}
        return rest, {n: np.linalg.inv(m) for n, m in rest.items()}

    def world_quat(self, name, w):
        """The pose (basis) quaternion that turns joint `name` by the rotation
        vector w (world axes at rest, radians), after its parent: as turn()."""
        w = Vector(w)
        a = w.length
        if a < 1e-12:
            return Quaternion()
        local = self.rig_obj.data.bones[name].matrix_local.to_3x3().inverted() @ (w / a)
        return Quaternion(local.normalized(), a)

    def chain(self, S, rotvecs):
        """The wrist (head of hand.S) with the shoulder, upper arm and forearm
        of side S turned by rotation vectors in world axes at rest (float64,
        the same composition as Blender's pose)."""
        rest, inv = self._rest
        M = rest["root"]
        prev = "root"
        for n in (f"shoulder.{S}", f"upper_arm.{S}", f"forearm.{S}", f"hand.{S}"):
            B = np.eye(4)
            w = rotvecs.get(n)
            if w is not None:
                R = rest[n][:3, :3]
                B[:3, :3] = R.T @ rodrigues(w) @ R
            M = M @ inv[prev] @ rest[n] @ B
            prev = n
        return M[:3, 3]

    def carry(self, spec, phase, S, guess):
        """One side's joint turns at `phase` of a clip: the forearm rocks with
        the swing, the hand trails it, and the shoulder and upper arm turn
        (least turn, weighted by SOLVE_WEIGHT) to put the wrist on its target.
        Returns (name -> basis quaternion, the solved rotation vectors)."""
        sd = self.sides[S]
        sgn = 1.0 if S == "L" else -1.0
        c1, s1 = math.cos(2 * math.pi * phase), math.sin(2 * math.pi * phase)
        dy = spec["lift"] - spec["bob"] * math.cos(4 * math.pi * phase)   # engine y: up
        dz = sgn * spec["swing"] * c1                                      # engine z: forward
        target = np.array(sd.W + Vector((0.0, -dz, dy)), dtype=np.float64)
        # The elbow's hinge: a turn by -a about it opens the elbow by a (the
        # opened test pose); the hand pitches about the same hinge, in the
        # arm's plane. The forearm opens as the hand swings forward; the hand
        # lags the forearm by a quarter cycle's worth of its own swing.
        hinge = np.array(sd.au.cross(sd.af).normalized(), dtype=np.float64)
        opened = sgn * spec["elbow"] * c1
        trail = sgn * spec["wrist"] * s1
        fixed = {f"forearm.{S}": hinge * math.radians(-opened), f"hand.{S}": hinge * math.radians(-trail)}
        wgt = np.array([SOLVE_WEIGHT[0]] * 3 + [SOLVE_WEIGHT[1]] * 3)

        def wrist(x):
            rv = dict(fixed)
            rv[f"shoulder.{S}"] = x[:3]
            rv[f"upper_arm.{S}"] = x[3:]
            return self.chain(S, rv)

        # Least weighted turn that reaches the target: Gauss-Newton steps on
        # the constraint, each landing on the least-norm point of its linearisation.
        x = np.array(guess, dtype=np.float64)
        for _ in range(50):
            f = wrist(x)
            r = target - f
            if np.linalg.norm(r) < 1e-12:
                break
            J = np.empty((3, 6))
            for i in range(6):
                xe = x.copy()
                xe[i] += 1e-7
                J[:, i] = (wrist(xe) - f) / 1e-7
            Wi = 1.0 / wgt
            mu = np.linalg.solve((J * Wi) @ J.T, r + J @ x)
            x = Wi * (J.T @ mu)
        if np.linalg.norm(target - wrist(x)) > 1e-7:
            raise RuntimeError(f"{S} wrist misses its target at phase {phase:.3f} by {np.linalg.norm(target - wrist(x))}")
        q = {f"shoulder.{S}": self.world_quat(f"shoulder.{S}", x[:3]),
             f"upper_arm.{S}": self.world_quat(f"upper_arm.{S}", x[3:]),
             f"forearm.{S}": self.world_quat(f"forearm.{S}", fixed[f"forearm.{S}"]),
             f"hand.{S}": self.world_quat(f"hand.{S}", fixed[f"hand.{S}"])}
        if spec["fist"]:
            for name, *_ in FINGERS:
                hinge_f = sd.fingers[name][2]
                for i in range(3):
                    q[f"{name}.{i + 1}.{S}"] = self.world_quat(f"{name}.{i + 1}.{S}", hinge_f * math.radians(spec["fist"]))
            for i in range(3):
                q[f"thumb.{i + 1}.{S}"] = self.world_quat(f"thumb.{i + 1}.{S}", sd.thumb[2] * math.radians(spec["fist"]))
        return q, x

    def clip_keys(self, spec):
        """The clip's 31 keys: name -> basis quaternion for every joint (rest
        is the identity). The last key is the first, so it loops."""
        keys = []
        guess = {S: np.zeros(6) for S in "LR"}
        for frame in range(CLIP_FRAMES):
            qs = {n: Quaternion() for n in JOINTS}
            for S in "LR":
                q, guess[S] = self.carry(spec, frame / CLIP_FRAMES, S, guess[S])
                qs.update(q)
            for n, qq in qs.items():
                if qq.w < 0:
                    qs[n] = -qq
            keys.append(qs)
        keys.append(keys[0])
        return keys

    def set_pose(self, qs):
        for pb in self.rig_obj.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = qs[pb.name]
            pb.location = Vector()
            pb.scale = Vector((1, 1, 1))

    def animate(self):
        """The clips, as actions on the rig: every joint keyed on every frame
        0..30 (rotation, location, scale), linear, the last key the first."""
        scene = bpy.context.scene
        scene.render.fps, scene.render.fps_base = FPS, 1.0
        self._rest = self.rest_np()
        self.keys = {name: self.clip_keys(CLIPS[name]) for name in CLIP_ORDER}
        rig = self.rig_obj
        rig.animation_data_create()
        for name in CLIP_ORDER:
            old = bpy.data.actions.get(name)
            if old:
                bpy.data.actions.remove(old)
            act = bpy.data.actions.new(name)
            act.use_fake_user = True
            rig.animation_data.action = act
            for frame, qs in enumerate(self.keys[name]):
                self.set_pose(qs)
                for pb in rig.pose.bones:
                    pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
                    pb.keyframe_insert("location", frame=frame, group=pb.name)
                    pb.keyframe_insert("scale", frame=frame, group=pb.name)
            for fc in action_fcurves(act):
                for kp in fc.keyframe_points:
                    kp.interpolation = "LINEAR"
                fc.update()
            act.use_frame_range = True
            act.frame_start, act.frame_end = 0, CLIP_FRAMES
            act.use_cyclic = True
            rig.animation_data.action = None
        self.clear_pose()

    def sample(self, name, frame):
        """The clip's pose at `frame` (0..30, fractional between keys), as the
        engine samples it: each joint's rotation slerped the short way."""
        keys = self.keys[name]
        k = min(int(math.floor(frame)), CLIP_FRAMES - 1)
        f = frame - k
        out = {}
        for n in JOINTS:
            a, c = keys[k][n], keys[k + 1][n]
            if f == 0.0:
                out[n] = a.copy()
                continue
            if a.dot(c) < 0:
                c = -c
            out[n] = a.slerp(c, f)
        return out


def rodrigues(w):
    """The rotation matrix of rotation vector w (float64)."""
    w = np.asarray(w, dtype=np.float64)
    a = np.linalg.norm(w)
    if a < 1e-15:
        return np.eye(3)
    k = w / a
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * (K @ K)


def action_fcurves(act):
    """An action's F-curves, under Blender 5's slotted actions or older ones."""
    if hasattr(act, "fcurves") and len(getattr(act, "fcurves")):
        return list(act.fcurves)
    out = []
    for layer in act.layers:
        for strip in layer.strips:
            for slot in act.slots:
                bag = strip.channelbag(slot)
                if bag:
                    out += list(bag.fcurves)
    return out


# --------------------------------------------------------------------------
# Checks and measurements


def piece_indices(b):
    """piece name -> (vertex indices, polygon indices), from the built mesh."""
    bm = b.bm
    bm.verts.index_update()
    bm.faces.index_update()
    return {name: (sorted(x.index for x in p.verts), sorted(f.index for f in p.faces)) for name, p in b.pieces.items()}


def closed_problems(b):
    problems = []
    for name, p in b.pieces.items():
        faces = set(p.faces)
        for f in p.faces:
            for e in f.edges:
                n = sum(1 for g in e.link_faces if g in faces)
                if n != 2:
                    problems.append(f"{name} is not closed (an edge with {n} faces)")
                    break
            else:
                continue
            break
        vol = 0.0
        for f in p.faces:
            co = [x.co for x in f.verts]
            for i in range(1, len(co) - 1):
                vol += co[0].dot(co[i].cross(co[i + 1]))
        if vol <= 0:
            problems.append(f"{name} faces inward (volume {vol / 6:.3g})")
    return problems


def band_crease_problems(b):
    """Each band's lip and foot edges turn more than CREASE_MIN degrees."""
    problems, worst = [], {}
    for S, band_loops, all_loops in b.band_edges:
        idx = {id(l[0]): i for i, l in enumerate(all_loops)}
        lo = 180.0
        for loop in band_loops:
            i = idx[id(loop[0])]
            if i == 0 or i == len(all_loops) - 1:
                continue
            A, B, C = all_loops[i - 1], loop, all_loops[i + 1]
            for k in range(SIDES):
                j = (k + 1) % SIDES
                n1 = (A[j].co - A[k].co).cross(B[k].co - A[k].co)
                n2 = (B[j].co - B[k].co).cross(C[k].co - B[k].co)
                ang = math.degrees(n1.angle(n2))
                lo = min(lo, ang)
        worst[S] = round(lo, 1)
        if lo <= CREASE_MIN:
            problems.append(f"{S} forearm: a band edge turns only {lo:.1f} degrees (needs > {CREASE_MIN})")
    return problems, worst


def evaluated_points(b):
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    ev = b.obj.evaluated_get(dg)
    me = ev.to_mesh()
    pts = [b.obj.matrix_world @ x.co for x in me.vertices]
    ev.to_mesh_clear()
    return pts


def attachment_problems(b, pose_name, pts, idx, polys):
    problems = []

    def piece_tree(vi, pi):
        # Each piece's tree from its own vertices only, so the hundreds of
        # small pieces don't each copy the whole mesh.
        local = {g: k for k, g in enumerate(vi)}
        return BVHTree.FromPolygons([pts[g] for g in vi], [[local[g] for g in polys[i]] for i in pi])
    trees = {name: piece_tree(vi, pi) for name, (vi, pi) in idx.items()}

    def inside(p, name):
        hits, o, d = 0, p.copy(), Vector((0.31, 0.22, 0.925)).normalized()
        for _ in range(32):
            hit, _, _, _ = trees[name].ray_cast(o, d)
            if hit is None:
                break
            hits += 1
            o = hit + d * 1e-6
        return hits % 2 == 1

    def touch(a, c):
        if trees[a].overlap(trees[c]):
            return True
        return inside(pts[idx[a][0][0]], c) or inside(pts[idx[c][0][0]], a)

    for a, c in b.attach:
        if not touch(a, c):
            problems.append(f"{pose_name}: {a} does not touch {c} (a gap or a floating piece)")
    for name, loop, hosts in b.covered:
        loose = [x for x in loop if not any(inside(pts[x.index], h) for h in hosts)]
        if loose:
            c = pts[loose[0].index]
            problems.append(f"{pose_name}: {len(loose)} end verts of {name} show outside {hosts} "
                            f"near ({c.x:.3f}, {c.y:.3f}, {c.z:.3f})")
    # Every piece is reached from its arm tube through the attachments.
    links = {}
    for a, c in b.attach:
        links.setdefault(a, set()).add(c)
        links.setdefault(c, set()).add(a)
    for S in "LR":
        reach, stack = {f"{S}.arm"}, [f"{S}.arm"]
        while stack:
            for j in links.get(stack.pop(), ()):
                if j not in reach:
                    reach.add(j)
                    stack.append(j)
        for name in idx:
            if name.startswith(S + ".") and name not in reach:
                problems.append(f"{pose_name}: {name} is attached to nothing")
    return problems


def framing(b, pts, idx):
    """The framing through the eye camera, and the problems with it."""
    problems = []
    m = {}
    # Every vertex, and points along every edge, inside the view: in the lower 35% and 0.15 m ahead.
    samples = list(pts)
    me = b.obj.data
    for e in me.edges:
        a, c = pts[e.vertices[0]], pts[e.vertices[1]]
        for k in (0.25, 0.5, 0.75):
            samples.append(a.lerp(c, k))
    top, near = -2.0, 10.0
    for p in samples:
        q = project(p)
        if q is None or abs(q[0]) > 1 or abs(q[1]) > 1:
            continue
        top = max(top, q[1])
        near = min(near, q[2])
    m["highest_ndc_y_in_view"] = round(top, 3)
    m["nearest_in_view_m"] = round(near, 3)
    if top > FRAME_TOP:
        problems.append(f"something in view reaches NDC y {top:.3f}, above {FRAME_TOP}")
    if near < CLEARANCE:
        problems.append(f"something in view is only {near:.3f} m ahead of the eye (needs {CLEARANCE})")
    for S in "LR":
        sd = b.sides[S]
        # Index knuckle: the top of index.1 near its joint.
        K = sd.fingers["index"][0][0]
        d = sd.fingers["index"][1][0][0]
        ys = [project(pts[i])[1] for i in idx[f"{S}.index.1"][0] if (pts[i] - K).dot(d) <= 0.012]
        ky = max(ys)
        m[f"index_knuckle_top_ndc_y.{S}"] = round(ky, 3)
        if not KNUCKLE_Y[0] <= ky <= KNUCKLE_Y[1]:
            problems.append(f"{S} index knuckle top at NDC y {ky:.3f}, outside {KNUCKLE_Y}")
        # Hand centre: the middle of the hand's screen box (palm, fingers, thumb).
        hand = [n for n in idx if n.startswith(S + ".") and n.split(".")[1] in
                ("palm", "thumb", "index", "middle", "ring", "pinky")]
        qs = [project(pts[i]) for n in hand for i in idx[n][0]]
        xs, ys2 = [q[0] for q in qs], [q[1] for q in qs]
        cx, cy = (min(xs) + max(xs)) / 2, (min(ys2) + max(ys2)) / 2
        m[f"hand_centre_ndc.{S}"] = [round(cx, 3), round(cy, 3)]
        m[f"hand_box_ndc.{S}"] = [round(min(xs), 3), round(max(xs), 3), round(min(ys2), 3), round(max(ys2), 3)]
        want_sign = -1 if S == "L" else 1
        if cx * want_sign <= 0 or not HAND_X[0] <= abs(cx) <= HAND_X[1]:
            problems.append(f"{S} hand centre at NDC x {cx:.3f}, wants {'-' if S == 'L' else '+'}{HAND_X}")
        # Forearm leaves through the bottom edge: walk its axis from the wrist to the elbow.
        exit_q = None
        for i in range(1, 401):
            p = sd.W.lerp(sd.E, i / 400)
            q = project(p)
            if q is None or abs(q[0]) > 1 or abs(q[1]) > 1:
                exit_q = q
                break
        if exit_q is None or exit_q[1] >= -1.0 or abs(exit_q[0]) > 1.0:
            problems.append(f"{S} forearm does not leave through the bottom edge (at {exit_q})")
        else:
            m[f"forearm_exit_ndc_x.{S}"] = round(exit_q[0], 3)
        m[f"wrist_ndc.{S}"] = [round(c, 3) for c in project(sd.W)]
    # The gauge's readout: the screen and every dot in view, every dot seen,
    # wide enough both ways, facing the eye, and reading left to right and
    # top to bottom on screen.
    tree = BVHTree.FromPolygons(pts, [p.vertices for p in me.polygons])
    poly_piece = {}
    for name, (vi, pi) in idx.items():
        for i in pi:
            poly_piece[i] = name

    def px_between(a, c):
        qa, qc = project(a), project(c)
        return math.hypot((qa[0] - qc[0]) * VIEW_W / 2, (qa[1] - qc[1]) * VIEW_H / 2)
    for name in ["L.screen"] + [d[0] for d in b.lcd]:
        if not all(in_view(pts[i]) for i in idx[name][0]):
            problems.append(f"{name} is not wholly in view")
    px_across, px_up, facing_min = [], [], 1.0
    glyph_x = {}
    row_y = {}
    for name, g, r, top_c, right, up, n in b.lcd:
        hit, _, pidx, _ = tree.ray_cast(Vector((0, 0, 0)), top_c.normalized(), top_c.length + 0.01)
        dot_seen = hit is not None and poly_piece.get(pidx) == name
        across = px_between(top_c - right * LCD_DOT / 2, top_c + right * LCD_DOT / 2)
        upward = px_between(top_c - up * LCD_DOT / 2, top_c + up * LCD_DOT / 2)
        facing = n.dot((-top_c).normalized())
        px_across.append(across)
        px_up.append(upward)
        facing_min = min(facing_min, facing)
        if not dot_seen or across < LCD_MIN_PX or upward < LCD_MIN_PX or facing < FACE_COS:
            problems.append(f"{name}: seen from the eye {dot_seen}, {across:.1f} x {upward:.1f} px, facing {facing:.3f}")
        q = project(top_c)
        glyph_x.setdefault(g, []).append(q[0])
        row_y.setdefault((g, r), []).append(q[1])
    gx = [sum(glyph_x[g]) / len(glyph_x[g]) for g in range(len(LCD_TEXT))]
    if not all(a < c for a, c in zip(gx, gx[1:])):
        problems.append(f"the readout's glyphs don't read left to right: mean NDC x {[round(x, 4) for x in gx]}")
    for g in range(len(LCD_TEXT)):
        if min(row_y[(g, 0)]) <= max(row_y[(g, 4)]):
            problems.append(f"the readout's glyph {g + 1}: row 1 does not project above row 5")
    lq = [project(pts[i]) for d in b.lcd for i in idx[d[0]][0]]
    m["lcd"] = {"dots": len(b.lcd),
                "ndc": {"x": [round(min(q[0] for q in lq), 3), round(max(q[0] for q in lq), 3)],
                        "y": [round(min(q[1] for q in lq), 3), round(max(q[1] for q in lq), 3)]},
                "px_min": [round(min(px_across), 2), round(min(px_up), 2)],
                "facing_min": round(facing_min, 3)}

    def verts(name):
        return [pts[i] for i in idx[name][0]]

    def piece_seen(name):
        return any(seen(tree, p) for p in verts(name))

    def width(name):
        wd = screen_width(verts(name))
        return None if wd is None else round(wd, 1)
    # The compass: the face (dial, ticks, needle and its hub, lamp) wholly in
    # view, turned to the eye, wide enough, and the first thing the eye meets.
    ci = b.compass_info
    face = set(ci["face"])
    face_pts = [p for name in ci["face"] for p in verts(name)]
    out = [p for p in face_pts if not in_view(p)]
    if out:
        problems.append(f"{len(out)} compass face vertices are out of view")
    centre = ci["centre"]
    cos = ci["normal"].dot((-centre).normalized())
    blocked = {}
    for p in face_pts:
        # Only what lies before the vertex (by more than SEEN_TOL) can hide it.
        hit, pidx = first_hit(tree, p)
        if hit is not None and hit.length < p.length - SEEN_TOL and poly_piece.get(pidx) not in face:
            blocked[poly_piece.get(pidx)] = blocked.get(poly_piece.get(pidx), 0) + 1
    if blocked:
        problems.append(f"the compass face is hidden from the eye at {sum(blocked.values())} vertices by {sorted(blocked)}")
    dial_px, lamp_px = width(ci["dial"]), width(ci["lamp"])
    lid_top = max(project(p)[1] for p in verts(ci["lid"]))
    cq = project(centre)
    m["compass"] = {"centre_ndc": [round(cq[0], 3), round(cq[1], 3)], "centre_m_ahead": round(cq[2], 3),
                    "normal_cos_to_eye": round(cos, 3), "dial_px_wide": dial_px, "lamp_px_wide": lamp_px,
                    "face_vertices": len(face_pts), "face_vertices_blocked": sum(blocked.values()),
                    "lid_top_ndc_y": round(lid_top, 3), "mount_angle_deg": ci["angle"],
                    "centre_m_behind_wrist": round(-(centre - b.sides["R"].W).dot(b.sides["R"].af), 4),
                    "housing_m_behind_wrist": [round(-max((p - b.sides["R"].W).dot(b.sides["R"].af)
                                                          for p in verts("R.compass_housing")), 4),
                                               round(-min((p - b.sides["R"].W).dot(b.sides["R"].af)
                                                          for p in verts("R.compass_housing")), 4)]}
    if cos < FACE_COS:
        problems.append(f"the compass face's normal is at cos {cos:.3f} to the eye (needs {FACE_COS})")
    if dial_px is None or dial_px < FACE_MIN_PX:
        problems.append(f"the compass dial is {dial_px} px wide (needs {FACE_MIN_PX})")
    if lamp_px is None or lamp_px < SMALL_MIN_PX:
        problems.append(f"the compass lamp is {lamp_px} px wide (needs {SMALL_MIN_PX})")
    if lid_top > FRAME_TOP:
        problems.append(f"the compass lid reaches NDC y {lid_top:.3f}, above {FRAME_TOP}")
    # Seen: the knurled dial, the three toggles, at least 2 of the 4 bezel screws.
    seen_m = {"knurl_dial": piece_seen("L.knurl_dial"),
              "toggles": [any(piece_seen(n) for n in names) for names in b.toggles],
              "screws": [piece_seen(n) for n in b.screws]}
    m["seen"] = seen_m
    if not seen_m["knurl_dial"]:
        problems.append("the knurled dial is not seen")
    if not all(seen_m["toggles"]) or len(seen_m["toggles"]) != 3:
        problems.append(f"the toggles seen: {seen_m['toggles']} (all three must be)")
    if sum(seen_m["screws"]) < 2:
        problems.append(f"only {sum(seen_m['screws'])} bezel screws are seen (needs 2 of 4)")
    # Small parts: every seen one at least SMALL_MIN_PX wide on screen.
    small = [n for n, s in zip(b.screws, seen_m["screws"]) if s]
    small += [names[2] for names in b.toggles]
    small += [ci["lamp"], "R.compass_needle", "L.knurl_mark"]
    small += [n for n in ci["face"] if n.startswith("R.compass_tick.")]
    widths = {}
    for name in small:
        if not piece_seen(name):
            continue
        wd = width(name)
        widths[name] = wd
        if wd is None or wd < SMALL_MIN_PX:
            problems.append(f"{name} is {wd} px wide on screen (needs {SMALL_MIN_PX})")
    m["small_part_px"] = widths
    # The knurl pitch: between the centres of neighbouring seen ridges.
    kseen = [seen(tree, p) for p in b.knurl]
    pitches = []
    for j, p in enumerate(b.knurl):
        jj = (j + 1) % len(b.knurl)
        if kseen[j] and kseen[jj]:
            a, c = to_px(project(p)), to_px(project(b.knurl[jj]))
            pitches.append(math.hypot(a[0] - c[0], a[1] - c[1]))
    m["knurl"] = {"ridges": len(b.knurl), "ridges_seen": sum(kseen),
                  "pitch_px_min": round(min(pitches), 2) if pitches else None,
                  "pitch_px_max": round(max(pitches), 2) if pitches else None}
    if len(pitches) < 2 or min(pitches) < SMALL_MIN_PX:
        problems.append(f"the knurl pitch is {m['knurl']['pitch_px_min']} px over {len(pitches)} seen pairs "
                        f"(needs {SMALL_MIN_PX})")
    return problems, m


def check(b):
    """Every binding check. Returns (problems, measurements)."""
    problems = []
    m = {}
    # Names and material order.
    objs = sorted(o.name for o in b.coll.objects)
    if objs != sorted([MESH, RIG]):
        problems.append(f"objects {objs} are not {sorted([MESH, RIG])}")
    mats = [mt.name for mt in b.obj.data.materials]
    if mats != [n for n, _, _ in MATERIALS]:
        problems.append(f"materials {mats} are not in the order {[n for n, _, _ in MATERIALS]}")
    colours = {}
    for (name, srgb, glow), mt in zip(MATERIALS, b.obj.data.materials):
        bsdf = mt.node_tree.nodes["Principled BSDF"]
        back = tuple(round(to_srgb(c) * 255) for c in bsdf.inputs["Base Color"].default_value[:3])
        emits = bsdf.inputs["Emission Strength"].default_value > 0
        colours[name] = {"srgb": back, "emissive": emits}
        if back != srgb:
            problems.append(f"{name} converts back to {back}, not {srgb}")
        if emits != glow:
            problems.append(f"{name} emissive {emits}, wants {glow}")
    m["materials"] = colours
    bones = [bn.name for bn in b.rig_obj.data.bones]
    if sorted(bones) != sorted(JOINTS) or len(bones) != 39:
        problems.append(f"joints {sorted(set(bones) ^ set(JOINTS))} differ from the list ({len(bones)} joints)")
    root = b.rig_obj.data.bones["root"]
    if root.head_local.length > 1e-6:
        problems.append(f"root joint at {tuple(root.head_local)}, not the origin")
    for o in (b.obj, b.rig_obj):
        if any(abs(o.matrix_world[i][j] - (1.0 if i == j else 0.0)) > 1e-6 for i in range(4) for j in range(4)):
            problems.append(f"{o.name} has a transform; the origin must be the eye")
    unweighted = [x.index for x in b.obj.data.vertices if not x.groups]
    if unweighted:
        problems.append(f"{len(unweighted)} vertices have no weights")
    # Triangles.
    tris = sum(len(p.vertices) - 2 for p in b.obj.data.polygons)
    m["triangles"] = tris
    by = {}
    for name, p in b.pieces.items():
        key = name.split(".")[1].split("_")[0]
        by[key] = by.get(key, 0) + sum(len(f.verts) - 2 for f in p.faces)
    m["triangles_by_part"] = by
    if not TRI_BUDGET[0] <= tris <= TRI_BUDGET[1]:
        problems.append(f"{tris} triangles, outside {TRI_BUDGET}")
    if any(p.use_smooth for p in b.obj.data.polygons):
        problems.append("some faces are smooth-shaded")
    used = sorted({p.material_index for p in b.obj.data.polygons})
    if used != list(range(len(MATERIALS))):
        problems.append(f"material slots used {used}: every one of the {len(MATERIALS)} must be (the exporter drops unused ones)")
    # Round parts in a creased material take at least ROUND_MIN sides.
    rounds = {name: p.sides for name, p in b.pieces.items() if p.sides is not None and p.mat in CREASED}
    thin = {name: n for name, n in rounds.items() if n < ROUND_MIN}
    m["round_parts_in_creased_materials"] = {"count": len(rounds), "fewest_sides": min(rounds.values())}
    if thin:
        problems.append(f"round parts in creased materials with under {ROUND_MIN} sides: {thin}")
    # Stitch dashes: at least 2.5 mm wide and 4 mm long, measured on their vertices.
    lo_len, lo_wid = 1.0, 1.0
    for name, along, across in b.stitches:
        cs = [x.co for x in b.pieces[name].verts]
        L = max(c.dot(along) for c in cs) - min(c.dot(along) for c in cs)
        Wd = max(c.dot(across) for c in cs) - min(c.dot(across) for c in cs)
        lo_len, lo_wid = min(lo_len, L), min(lo_wid, Wd)
    m["stitches"] = {"dashes": len(b.stitches), "shortest_mm": round(lo_len * 1000, 2), "narrowest_mm": round(lo_wid * 1000, 2)}
    if lo_len < 0.004 - 1e-7 or lo_wid < 0.0025 - 1e-7:
        problems.append(f"a stitch dash is {lo_len * 1000:.2f} mm long and {lo_wid * 1000:.2f} mm wide (needs 4 and 2.5)")
    # Gadget parts rigid to their forearm: every vertex weighted 1 to it alone.
    gadgets = [p for p in b.pieces.values() if p.gadget]
    loose = []
    for p in gadgets:
        g = b.group[p.gadget]
        for x in p.verts:
            ws = dict(x[b.deform].items())
            if set(ws) != {g} or abs(ws[g] - 1.0) > 1e-6:
                loose.append(p.name)
                break
    m["rigid_gadget_parts"] = len(gadgets)
    if loose:
        problems.append(f"gadget parts not rigid to their forearm: {loose}")
    # Small parts' sizes as built.
    m["screw_head_mm"], m["screw_slot_mm"] = SCREW[0] * 1000, SCREW[1] * 1000
    # Closed volumes and band creases.
    problems += closed_problems(b)
    cp, worst = band_crease_problems(b)
    problems += cp
    m["band_edge_min_turn_deg"] = worst
    m["bands"] = {S: {"boundaries_m": [round(x, 4) for x in b.sides[S].bands[0]],
                      "lips_mm": [round(h * 1000, 1) for h in b.sides[S].bands[1]]} for S in "LR"}
    # Poses: framing at rest, attachment in every test pose.
    idx = piece_indices(b)
    polys = [tuple(p.vertices) for p in b.obj.data.polygons]
    m["pieces"] = len(idx)
    poses = b.test_poses()
    b.clear_pose()
    rest_pts = evaluated_points(b)
    fp, fm = framing(b, rest_pts, idx)
    problems += fp
    m["framing"] = fm
    for name, setter in poses.items():
        b.clear_pose()
        setter()
        pts = evaluated_points(b)
        if name.startswith("elbows "):
            for S in "LR":
                sd = b.sides[S]
                pb = b.rig_obj.pose.bones[f"forearm.{S}"]
                head = b.rig_obj.matrix_world @ pb.head
                tail = b.rig_obj.matrix_world @ pb.tail
                before = math.degrees((sd.S - sd.E).angle(sd.W - sd.E))
                after = math.degrees((sd.S - head).angle(tail - head))
                if name == "elbows opened 30":
                    m[f"elbow_angle_deg.{S}"] = [round(before, 1), round(after, 1)]
                    if after < before + 25:
                        problems.append(f"{S} elbow test pose does not open it ({before:.1f} -> {after:.1f})")
                else:
                    m[f"elbow_closed_angle_deg.{S}"] = [round(before, 1), round(after, 1)]
                    if not before - ELBOW_CLOSE_TEST - 1 <= after <= before - ELBOW_CLOSE_TEST + 1:
                        problems.append(f"{S} closed-elbow test pose does not close it by {ELBOW_CLOSE_TEST} "
                                        f"({before:.1f} -> {after:.1f})")
        problems += attachment_problems(b, name, pts, idx, polys)
    b.clear_pose()
    m["poses_checked"] = list(poses)
    # Joints, for the record (engine coordinates).
    m["joints_engine"] = {n: [round(c, 3) for c in to_engine(b.rig_obj.data.bones[n].head_local)]
                          for n in ("shoulder.L", "upper_arm.L", "forearm.L", "hand.L", "index.1.L")}
    return problems, m


def to_engine(p):
    return (p[0], p[2], -p[1])


def posed_array(b):
    """The posed mesh's vertex positions as an (n, 3) array (the objects have identity transforms)."""
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    ev = b.obj.evaluated_get(dg)
    me = ev.to_mesh()
    arr = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", arr)
    ev.to_mesh_clear()
    return arr.reshape(-1, 3)


def view_extent(P, edges):
    """Over every vertex and every edge's quarter points in view: the highest
    NDC y and the nearest depth ahead of the eye (framing()'s rule, vectorised)."""
    Q = np.concatenate([P] + [P[edges[:, 0]] * (1 - k) + P[edges[:, 1]] * k for k in (0.25, 0.5, 0.75)])
    d = -Q[:, 1]
    Q, d = Q[d > 1e-9], d[d > 1e-9]
    x = -Q[:, 0] / (d * TAN_Y * ASPECT)
    y = Q[:, 2] / (d * TAN_Y)
    vis = (np.abs(x) <= 1.0) & (np.abs(y) <= 1.0)
    return float(y[vis].max()), float(d[vis].min())


def forearm_exit(W, E):
    """Where the forearm's axis, walked from the wrist to the elbow, leaves the view (NDC), or None."""
    for i in range(1, 401):
        q = project(W.lerp(E, i / 400))
        if q is None or abs(q[0]) > 1 or abs(q[1]) > 1:
            return q
    return None


def quat_deg(q):
    return math.degrees(2.0 * math.acos(min(1.0, abs(q.w))))


def phase_gap(a, c):
    d = abs(a - c) % 1.0
    return min(d, 1.0 - d)


def clip_checks(b):
    """Every binding check on the clips (task desert-arms-walk, Assets).
    Returns (problems, measurements)."""
    problems, m = [], {}
    rig = b.rig_obj
    bones = rig.data.bones
    idx = piece_indices(b)
    polys = [tuple(p.vertices) for p in b.obj.data.polygons]
    edges = np.empty(len(b.obj.data.edges) * 2, dtype=np.int64)
    b.obj.data.edges.foreach_get("vertices", edges)
    edges = edges.reshape(-1, 2)
    if rig.animation_data and rig.animation_data.action:
        problems.append("the rig has an active action; the checks pose it directly")
    for name in CLIP_ORDER:
        spec = CLIPS[name]
        keys = b.keys[name]
        cm = {"cycle_m": spec["cycle"], "keys": len(keys)}
        movers = {f"{j}.{S}" for j in LIMBS for S in "LR"}
        if spec["fist"]:
            movers |= {n for n in JOINTS if n.split(".")[0] in DIGITS}
        if len(keys) != CLIP_FRAMES + 1:
            problems.append(f"{name} has {len(keys)} keys, wants {CLIP_FRAMES + 1}")
        # Loops: the last key equals the first, exactly.
        for n in JOINTS:
            if tuple(keys[-1][n]) != tuple(keys[0][n]):
                problems.append(f"{name}: {n}'s last key is not its first")
        # Joints: everything but the movers keyed at exactly rest.
        still = [n for n in JOINTS if n not in movers and any(tuple(k[n]) != (1.0, 0.0, 0.0, 0.0) for k in keys)]
        if still:
            problems.append(f"{name}: joints that must stay at rest turn: {still}")
        moved = sorted({n for n in JOINTS if any(tuple(k[n]) != tuple(keys[0][n]) for k in keys)})
        cm["joints_changing_between_keys"] = moved
        # The actions as keyed: 31 keys a channel, linear, the last the first.
        act = bpy.data.actions.get(name)
        fcs = action_fcurves(act) if act else []
        if len(fcs) != len(JOINTS) * 10:
            problems.append(f"{name}: the action has {len(fcs)} F-curves, wants {len(JOINTS) * 10}")
        for fc in fcs:
            kps = fc.keyframe_points
            if len(kps) != CLIP_FRAMES + 1 or any(kp.interpolation != "LINEAR" for kp in kps) \
                    or kps[0].co[1] != kps[-1].co[1]:
                problems.append(f"{name}: {fc.data_path}[{fc.array_index}] is not 31 linear keys ending on its first")
                break
        # Steps: no joint turns more than STEP_MAX between adjacent keys.
        step = max(quat_deg(keys[k][n].rotation_difference(keys[k + 1][n])) for n in JOINTS for k in range(CLIP_FRAMES))
        cm["largest_key_step_deg"] = round(step, 2)
        if step > STEP_MAX:
            problems.append(f"{name}: a joint turns {step:.1f} degrees between adjacent keys (max {STEP_MAX})")
        # Every sampled frame (keys and half-keys, as the engine samples).
        rest_head = {S: bones[f"hand.{S}"].head_local.copy() for S in "LR"}
        rest_elbow = {S: math.degrees((b.sides[S].S - b.sides[S].E).angle(b.sides[S].W - b.sides[S].E)) for S in "LR"}
        off = {S: [] for S in "LR"}
        tops, nears, exits = [], [], {S: [] for S in "LR"}
        hand_deg, hand_axes, finger_deg = 0.0, {"twist": 0.0, "flex": 0.0, "sideways": 0.0}, 0.0
        elbow = {S: [1e9, -1e9] for S in "LR"}
        for i in range(2 * CLIP_FRAMES + 1):
            qs = b.sample(name, i / 2)
            b.set_pose(qs)
            P = posed_array(b)
            top, near = view_extent(P, edges)
            tops.append(top)
            nears.append(near)
            for S in "LR":
                sd = b.sides[S]
                pbs = rig.pose.bones
                Wp, Ep, Sp = pbs[f"hand.{S}"].head.copy(), pbs[f"forearm.{S}"].head.copy(), pbs[f"upper_arm.{S}"].head.copy()
                dlt = Wp - rest_head[S]
                off[S].append((dlt.z, -dlt.y))
                q = forearm_exit(Wp, Ep)
                if q is None or q[1] >= -1.0 or abs(q[0]) > 1.0:
                    problems.append(f"{name} frame {i / 2}: {S} forearm does not leave through the bottom edge (at {q})")
                else:
                    exits[S].append(q[0])
                ang = math.degrees((Sp - Ep).angle(Wp - Ep)) - rest_elbow[S]
                elbow[S] = [min(elbow[S][0], ang), max(elbow[S][1], ang)]
                hq = qs[f"hand.{S}"]
                hand_deg = max(hand_deg, quat_deg(hq))
                # The hand's turn as a rotation vector in world axes at rest, about the test poses' axes.
                ax, a = hq.to_axis_angle()
                if a > math.pi:
                    a -= 2 * math.pi
                wv = bones[f"hand.{S}"].matrix_local.to_3x3() @ ax * math.degrees(a)
                for kind, axis in (("twist", sd.af), ("flex", sd.t), ("sideways", sd.n)):
                    hand_axes[kind] = max(hand_axes[kind], abs(wv.dot(axis)))
                for n in JOINTS:
                    if n.endswith("." + S) and n.split(".")[0] in DIGITS:
                        finger_deg = max(finger_deg, quat_deg(qs[n]))
        b.clear_pose()
        top, near = max(tops), min(nears)
        cm["highest_ndc_y_in_view"] = round(top, 3)
        cm["nearest_in_view_m"] = round(near, 3)
        cm["forearm_exit_ndc_x"] = {S: [round(min(exits[S]), 3), round(max(exits[S]), 3)] for S in "LR" if exits[S]}
        if top > FRAME_TOP:
            problems.append(f"{name}: something in view reaches NDC y {top:.3f}, above {FRAME_TOP} "
                            f"(at frame {tops.index(top) / 2})")
        if near < CLEARANCE:
            problems.append(f"{name}: something in view is only {near:.3f} m ahead of the eye (needs {CLEARANCE})")
        # Ranges.
        cm["hand_turn_deg"] = {"total": round(hand_deg, 2), **{k: round(x, 2) for k, x in hand_axes.items()}}
        cm["elbow_opened_deg"] = {S: [round(x, 2) for x in elbow[S]] for S in "LR"}
        cm["finger_turn_deg"] = round(finger_deg, 2)
        if hand_deg > WRIST_MAX:
            problems.append(f"{name}: a hand turns {hand_deg:.1f} degrees from rest (max {WRIST_MAX} about each axis)")
        if finger_deg > FINGER_MAX:
            problems.append(f"{name}: a finger joint turns {finger_deg:.1f} degrees from rest (max {FINGER_MAX})")
        for S in "LR":
            if elbow[S][0] < ELBOW_SPAN[0] or elbow[S][1] > ELBOW_SPAN[1]:
                problems.append(f"{name}: the {S} elbow goes {elbow[S]} degrees from rest (allowed {ELBOW_SPAN})")
        # Amplitudes and timing.
        phases = [i / (2 * CLIP_FRAMES) for i in range(2 * CLIP_FRAMES + 1)]
        tg = CLIP_TARGETS[name]
        want = {"y": (tg["mean"] + tg["bob"], tg["mean"] - tg["bob"]), "z": (tg["swing"], -tg["swing"])}
        amp = {}
        for S in "LR":
            ys = [o[0] for o in off[S]]
            zs = [o[1] for o in off[S]]
            amp[S] = {"y_max": round(max(ys), 4), "y_min": round(min(ys), 4), "y_mean": round(sum(ys[:-1]) / (len(ys) - 1), 4),
                      "z_max": round(max(zs), 4), "z_min": round(min(zs), 4)}
            for axis, vals in (("y", ys), ("z", zs)):
                hi, lo = want[axis]
                if abs(max(vals) - hi) > AMP_TOL or abs(min(vals) - lo) > AMP_TOL:
                    problems.append(f"{name}: {S} hand {axis} offset spans {min(vals):.4f}..{max(vals):.4f}, "
                                    f"wants {lo:.4f}..{hi:.4f} within {AMP_TOL}")
            if abs(amp[S]["y_mean"] - tg["mean"]) > AMP_TOL:
                problems.append(f"{name}: {S} hand y mean {amp[S]['y_mean']}, wants {tg['mean']}")
            zpeak = phases[zs.index(max(zs))]
            want_z = 0.0 if S == "L" else 0.5
            low = {}
            for centre in (0.0, 0.5):
                half = [k for k, ph in enumerate(phases) if phase_gap(ph, centre) < 0.25]
                kk = min(half, key=lambda k: ys[k])
                low[centre] = phases[kk]
            amp[S]["z_peak_phase"] = round(zpeak, 4)
            amp[S]["y_low_phases"] = [round(low[0.0], 4), round(low[0.5], 4)]
            if phase_gap(zpeak, want_z) > TIMING_TOL + 1e-9:
                problems.append(f"{name}: {S} hand furthest forward at phase {zpeak:.3f}, wants {want_z} +- 1/30")
            for centre, ph in low.items():
                if phase_gap(ph, centre) > TIMING_TOL + 1e-9:
                    problems.append(f"{name}: {S} hand lowest near phase {centre} at {ph:.3f}, wants within 1/30")
        cm["hand_offsets_m"] = amp
        # Attachment at evenly spaced phases.
        for k in range(ATTACH_FRAMES):
            b.set_pose(b.sample(name, CLIP_FRAMES * k / ATTACH_FRAMES))
            pts = evaluated_points(b)
            problems += attachment_problems(b, f"{name} phase {k}/{ATTACH_FRAMES}", pts, idx, polys)
        b.clear_pose()
        cm["attachment_phases"] = [f"{k}/{ATTACH_FRAMES}" for k in range(ATTACH_FRAMES)]
        m[name] = cm
    return problems, m


# --------------------------------------------------------------------------
# Export, glb check and previews


def export():
    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_yup=True,
        export_normals=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_skins=True,
        export_animations=False,
        export_def_bones=False,
        export_apply=False,
    )
    print("wrote", OUT)


def add_clips(path, b):
    """Append the clips to the exported glb, which holds the mesh, skin and
    materials exactly as before. Every joint gets a translation, rotation and
    scale channel of 31 LINEAR keys at 0, 1/30 ... 1.0 s. Translation and
    scale keys, and the rotation keys of every joint the clip doesn't turn,
    are the joint node's own rest values, so they equal rest exactly. A turned
    joint's key is its node rotation times its pose quaternion (the glTF
    exporter's own composition, checked against it: bone axes are kept).
    Blender's exporter samples the same actions to within 2 float steps of
    rest, not exactly, so the build writes the clips itself."""
    with open(path, "rb") as fh:
        gj, binc = read_glb(fh.read())
    if gj.get("animations"):
        raise RuntimeError("the exported glb already has animations")
    nodes = gj["nodes"]
    joints = gj["skins"][0]["joints"]
    blob = bytearray(binc)
    while len(blob) % 4:
        blob.append(0)
    view = len(gj["bufferViews"])
    start = len(blob)
    accessors = gj["accessors"]

    def put(rows, kind):
        arr = np.asarray(rows, dtype="<f4")
        offset = len(blob) - start
        blob.extend(arr.tobytes())
        acc = {"bufferView": view, "byteOffset": offset, "componentType": 5126,
               "count": int(arr.shape[0]), "type": kind}
        if kind == "SCALAR":
            acc["min"], acc["max"] = [float(arr.min())], [float(arr.max())]
        accessors.append(acc)
        return len(accessors) - 1

    times = put([k / CLIP_FRAMES for k in range(CLIP_FRAMES + 1)], "SCALAR")
    anims = []
    for name in CLIP_ORDER:
        channels, samplers = [], []
        keys = b.keys[name]
        for j in joints:
            nd = nodes[j]
            jn = nd["name"]
            t = nd.get("translation", [0.0, 0.0, 0.0])
            sc = nd.get("scale", [1.0, 1.0, 1.0])
            x, y, z, w = nd.get("rotation", [0.0, 0.0, 0.0, 1.0])
            rows = []
            for k in range(CLIP_FRAMES + 1):
                qb = keys[k][jn]
                if tuple(qb) == (1.0, 0.0, 0.0, 0.0):
                    rows.append([x, y, z, w])
                    continue
                q = (Quaternion((w, x, y, z)) @ qb).normalized()
                if rows and np.dot(rows[-1], [q.x, q.y, q.z, q.w]) < 0:
                    q = -q
                rows.append([q.x, q.y, q.z, q.w])
            if rows[-1] != rows[0]:
                rows[-1] = list(rows[0])   # the last key is the first (keys[30] is keys[0])
            for target, data, kind in (("translation", [t] * (CLIP_FRAMES + 1), "VEC3"), ("rotation", rows, "VEC4"),
                                       ("scale", [sc] * (CLIP_FRAMES + 1), "VEC3")):
                samplers.append({"input": times, "interpolation": "LINEAR", "output": put(data, kind)})
                channels.append({"sampler": len(samplers) - 1, "target": {"node": j, "path": target}})
        anims.append({"name": name, "channels": channels, "samplers": samplers})
    gj["bufferViews"].append({"buffer": 0, "byteOffset": start, "byteLength": len(blob) - start})
    gj["animations"] = anims
    gj["buffers"][0]["byteLength"] = len(blob)
    js = json.dumps(gj, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js
    out += struct.pack("<II", len(blob), 0x004E4942) + bytes(blob)
    with open(path, "wb") as fh:
        fh.write(out)
    print("added", ", ".join(CLIP_ORDER), "to", path)


def check_glb(path, tris_expected):
    """Read the glb back: one skinned mesh, 39 named joints, the 15
    materials in order with their colours, only the glow, amber and screen emissive,
    and the two clips (checked further by check_glb_clips)."""
    with open(path, "rb") as fh:
        data = fh.read()
    magic, _, _ = struct.unpack_from("<III", data, 0)
    clen, ctype = struct.unpack_from("<II", data, 12)
    if magic != 0x46546C67 or ctype != 0x4E4F534A:
        return ["not a glb"], {}
    gj = json.loads(data[20:20 + clen])
    problems = []
    m = {}
    meshes = gj.get("meshes", [])
    skins = gj.get("skins", [])
    if len(meshes) != 1 or len(skins) != 1:
        problems.append(f"{len(meshes)} meshes and {len(skins)} skins, wants one of each")
    joints = [gj["nodes"][j]["name"] for j in skins[0]["joints"]] if skins else []
    if sorted(joints) != sorted(JOINTS):
        problems.append(f"glb joints differ: {sorted(set(joints) ^ set(JOINTS))}")
    mats = gj.get("materials", [])
    names = [mt["name"] for mt in mats]
    if names != [n for n, _, _ in MATERIALS]:
        problems.append(f"glb materials {names}")
    got = {}
    for mt, (name, srgb, glow) in zip(mats, MATERIALS):
        c = mt.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])
        back = tuple(round(to_srgb(x) * 255) for x in c[:3])
        e = mt.get("emissiveFactor", [0, 0, 0])
        glows = sum(e) > 0.01
        got[name] = {"srgb": back, "glow": glows}
        if back != srgb or glows != glow:
            problems.append(f"glb {name}: {back} glow {glows}, wants {srgb} glow {glow}")
    anims = sorted(a.get("name", "") for a in gj.get("animations", []))
    if anims != sorted(CLIP_ORDER):
        problems.append(f"glb animations {anims}, wants {sorted(CLIP_ORDER)}")
    tris = 0
    for mesh in meshes:
        for prim in mesh["primitives"]:
            tris += gj["accessors"][prim["indices"]]["count"] // 3
            for att in ("JOINTS_0", "WEIGHTS_0", "NORMAL"):
                if att not in prim["attributes"]:
                    problems.append(f"glb primitive lacks {att}")
    if tris != tris_expected:
        problems.append(f"glb has {tris} triangles, the build {tris_expected}")
    m.update({"glb_triangles": tris, "glb_joints": len(joints), "glb_materials": got,
              "glb_nodes": sorted(n.get("name", "") for n in gj["nodes"] if "mesh" in n or "skin" in n)})
    return problems, m


def read_glb(data):
    """A glb's JSON and binary chunk, from its bytes."""
    magic, _, _ = struct.unpack_from("<III", data, 0)
    clen, ctype = struct.unpack_from("<II", data, 12)
    if magic != 0x46546C67 or ctype != 0x4E4F534A:
        raise RuntimeError("not a glb")
    gj = json.loads(data[20:20 + clen])
    off = 20 + clen
    binc = b""
    if off + 8 <= len(data):
        blen, _ = struct.unpack_from("<II", data, off)
        binc = data[off + 8:off + 8 + blen]
    return gj, binc


def read_accessor(gj, binc, i):
    """Accessor i as an (count, components) float64 array of its stored values."""
    acc = gj["accessors"][i]
    bv = gj["bufferViews"][acc["bufferView"]]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[acc["type"]]
    dt = np.dtype({5126: "<f4", 5125: "<u4", 5123: "<u2", 5121: "u1"}[acc["componentType"]])
    stride = bv.get("byteStride", comps * dt.itemsize)
    start = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    rows = [np.frombuffer(binc, dt, comps, start + k * stride) for k in range(acc["count"])] \
        if stride != comps * dt.itemsize else [np.frombuffer(binc, dt, comps * acc["count"], start)]
    return np.concatenate(rows).astype(np.float64).reshape(acc["count"], comps)


def f32(xs):
    return tuple(float(np.float32(x)) for x in xs)


def node_trs(node):
    return (f32(node.get("translation", (0.0, 0.0, 0.0))), f32(node.get("rotation", (0.0, 0.0, 0.0, 1.0))),
            f32(node.get("scale", (1.0, 1.0, 1.0))))


def trs_matrix(t, r, sc):
    x, y, z, w = r
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4)
    M[:3, :3] = R * np.array(sc)
    M[:3, 3] = t
    return M


def check_glb_clips(data, before, b):
    """Read the clips back from the glb's bytes: exactly the two, each 1.0 s
    with 31 keys a channel, LINEAR, on every joint; the last key the first;
    translation and scale keys, and every joint a clip doesn't animate, at
    exactly the node's rest transform; the hands where the build put them;
    and the rest pose (vertex positions, joint nodes, inverse bind matrices)
    equal to `before`, the glb the clips were added to."""
    problems, m = [], {}
    gj, binc = read_glb(data)
    nodes = gj["nodes"]
    skin = gj["skins"][0]
    joint_of = {j: nodes[j]["name"] for j in skin["joints"]}
    by_name = {nm: j for j, nm in joint_of.items()}
    parent = {}
    for i, nd in enumerate(nodes):
        for c in nd.get("children", []):
            parent[c] = i
    rest = {j: node_trs(nodes[j]) for j in joint_of}

    def world(j, trs):
        M = trs_matrix(*trs[j]) if j in trs else trs_matrix(*node_trs(nodes[j]))
        return world(parent[j], trs) @ M if j in parent else M

    anims = {a.get("name", ""): a for a in gj.get("animations", [])}
    if sorted(anims) != sorted(CLIP_ORDER):
        problems.append(f"glb animations {sorted(anims)}, wants {sorted(CLIP_ORDER)}")
    for name in CLIP_ORDER:
        a = anims.get(name)
        if a is None:
            continue
        spec = CLIPS[name]
        movers = {f"{j}.{S}" for j in LIMBS for S in "LR"}
        if spec["fist"]:
            movers |= {n for n in JOINTS if n.split(".")[0] in DIGITS}
        chans = {}
        for ch in a["channels"]:
            node, path = ch["target"].get("node"), ch["target"]["path"]
            if node not in joint_of:
                problems.append(f"glb {name}: a {path} channel on node {nodes[node].get('name') if node is not None else None}, not a joint")
                continue
            smp = a["samplers"][ch["sampler"]]
            times = read_accessor(gj, binc, smp["input"])[:, 0]
            vals = read_accessor(gj, binc, smp["output"])
            chans[(joint_of[node], path)] = (smp.get("interpolation", "LINEAR"), times, vals)
        durations = set()
        for n in JOINTS:
            for path, k in (("translation", 0), ("rotation", 1), ("scale", 2)):
                c = chans.get((n, path))
                if c is None:
                    problems.append(f"glb {name}: {n} has no {path} channel")
                    continue
                interp, times, vals = c
                durations.add(round(float(times[-1]), 6))
                if interp != "LINEAR" or len(times) != CLIP_FRAMES + 1 or len(vals) != CLIP_FRAMES + 1:
                    problems.append(f"glb {name}: {n} {path} is {interp} with {len(times)} keys")
                    continue
                if times[0] != 0.0 or abs(times[-1] - 1.0) > 1e-6 or np.any(np.diff(times) <= 0):
                    problems.append(f"glb {name}: {n} {path} times run {times[0]}..{times[-1]}")
                if tuple(vals[0]) != tuple(vals[-1]):
                    problems.append(f"glb {name}: {n} {path}'s last key is not its first")
                if path != "rotation" or n not in movers:
                    if any(tuple(v) != rest[by_name[n]][k] for v in vals):
                        problems.append(f"glb {name}: {n} {path} keys are not exactly its rest {path}")
        m[f"{name}_duration_s"] = sorted(durations)
        m[f"{name}_channels"] = len(chans)
        # The hands where the build put them: each key's hand.L/R head offset,
        # computed from the glb's own transforms, against the build's (engine axes).
        worst = 0.0
        for kf in (0, 7, 15, 22):
            trs = {}
            for j, nm in joint_of.items():
                t = chans[(nm, "translation")][2][kf]
                r = chans[(nm, "rotation")][2][kf]
                sc = chans[(nm, "scale")][2][kf]
                trs[j] = (tuple(t), tuple(r), tuple(sc))
            b.set_pose(b.keys[name][kf])
            bpy.context.view_layer.update()
            for S in "LR":
                j = by_name[f"hand.{S}"]
                got = world(j, trs)[:3, 3] - world(j, {})[:3, 3]
                d = b.rig_obj.pose.bones[f"hand.{S}"].head - b.rig_obj.data.bones[f"hand.{S}"].head_local
                worst = max(worst, float(np.abs(got - np.array(to_engine(d))).max()))
        b.clear_pose()
        m[f"{name}_hand_offset_vs_build_m"] = worst
        if worst > 1e-5:
            problems.append(f"glb {name}: the hands are {worst:.2e} m from where the build put them")
    # The rest pose against the glb before the clips.
    if before is None:
        problems.append("no earlier glb to compare the rest pose with")
        return problems, m
    gb, bb = read_glb(before)

    def positions(g, bc):
        mesh = g["meshes"][0]
        return np.concatenate([read_accessor(g, bc, p["attributes"]["POSITION"]) for p in mesh["primitives"]])
    pa, pb_ = positions(gj, binc), positions(gb, bb)
    if pa.shape != pb_.shape:
        problems.append(f"rest-pose vertices: {pa.shape[0]} now, {pb_.shape[0]} before")
    else:
        dmax = float(np.abs(pa - pb_).max())
        m["rest_vertices"] = pa.shape[0]
        m["rest_vertex_max_diff_m"] = dmax
        if dmax > 1e-7:
            problems.append(f"rest-pose vertices moved by up to {dmax:.3g} m from the glb before the clips")
    nb = {nd.get("name"): nd for nd in gb["nodes"]}
    moved = [nodes[j]["name"] for j in joint_of if node_trs(nodes[j]) != node_trs(nb.get(nodes[j]["name"], {}))]
    ibm_a = read_accessor(gj, binc, skin["inverseBindMatrices"])
    ibm_b = read_accessor(gb, bb, gb["skins"][0]["inverseBindMatrices"])
    order_b = [gb["nodes"][j]["name"] for j in gb["skins"][0]["joints"]]
    order_a = [joint_of[j] for j in skin["joints"]]
    m["rest_joint_nodes_changed"] = moved
    if moved:
        problems.append(f"joint nodes' rest transforms changed: {moved}")
    if order_a != order_b or ibm_a.shape != ibm_b.shape or float(np.abs(ibm_a - ibm_b).max()) != 0.0:
        problems.append("the skin's joints or inverse bind matrices changed")
    m["rest_ibm_max_diff"] = float(np.abs(ibm_a - ibm_b).max()) if ibm_a.shape == ibm_b.shape else None
    return problems, m


def render_previews(b):
    """Four previews, Workbench with back faces culled as the engine draws:
    arms_eye.png    through the eye camera (60 degrees, 960x540): what the player sees
    arms.png        a three-quarter view from outside, front left and above
    arms_left.png   through the eye with a 20 degree field of view, aimed at the gauge's centre (960x540)
    arms_right.png  the same, aimed at the compass face's centre
    A sand-coloured ground plane 1.7 m below the eye is added for the previews only."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.studio_light = "outdoor.sl"
    sh.color_type = "MATERIAL"
    sh.show_backface_culling = True
    sh.show_object_outline = True
    sh.show_shadows = False
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "Standard"   # show the materials' sRGB colours as they are
    world = bpy.data.worlds.new("preview")
    world.color = (lin(156) , lin(192), lin(220))
    scene.world = world
    sand = bpy.data.materials.new("Preview_Sand")
    sand.diffuse_color = (lin(214), lin(176), lin(118), 1.0)
    gm = bpy.data.meshes.new("Preview_Ground")
    gm.from_pydata([(-200, -200, -1.7), (200, -200, -1.7), (200, 200, -1.7), (-200, 200, -1.7)], [], [(0, 1, 2, 3)])
    gm.materials.append(sand)
    ground = bpy.data.objects.new("Preview_Ground", gm)
    scene.collection.objects.link(ground)
    cam_data = bpy.data.cameras.new("Preview_Camera")
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle_y = FOV_Y
    cam_data.clip_start = NEAR
    cam_data.clip_end = 400.0
    cam = bpy.data.objects.new("Preview_Camera", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam

    def shoot(path, eye, target, res, up="Y"):
        scene.render.resolution_x, scene.render.resolution_y = res
        scene.render.resolution_percentage = 100
        cam.location = Vector(eye)
        cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", up).to_euler()
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("wrote", path)

    shoot(PREVIEW_EYE, (0.0, 0.0, 0.0), (0.0, -1.0, 0.0), (VIEW_W, VIEW_H))
    cam_data.angle_y = math.radians(20.0)
    shoot(PREVIEW_LEFT, (0.0, 0.0, 0.0), b.gauge_top, (VIEW_W, VIEW_H))
    shoot(PREVIEW_RIGHT, (0.0, 0.0, 0.0), b.compass_info["centre"], (VIEW_W, VIEW_H))
    cam_data.clip_start = 0.01
    cam_data.angle_y = math.radians(38.0)
    shoot(PREVIEW, (0.62, -0.98, 0.12), (0.03, -0.20, -0.20), (960, 720))
    # The clips: a 2x4 sheet of the eye view at phases 0, 1/8 ... 7/8 (top
    # row 0..3/8 left to right, bottom row 4/8..7/8), each sampled as the engine does.
    cam_data.clip_start = NEAR
    cam_data.angle_y = FOV_Y
    cols, rows, tw, th = SHEET
    tmp = os.path.join(OUT_DIR, ".arms_tile.png")
    for name in CLIP_ORDER:
        sheet = np.zeros((rows * th, cols * tw, 4), dtype=np.float32)
        for k in range(cols * rows):
            b.set_pose(b.sample(name, CLIP_FRAMES * k / (cols * rows)))
            shoot(tmp, (0.0, 0.0, 0.0), (0.0, -1.0, 0.0), (tw, th))
            img = bpy.data.images.load(tmp)
            px = np.empty(tw * th * 4, dtype=np.float32)
            img.pixels.foreach_get(px)
            bpy.data.images.remove(img)
            r, c = divmod(k, cols)
            y0 = (rows - 1 - r) * th   # pixel rows run bottom up
            sheet[y0:y0 + th, c * tw:(c + 1) * tw] = px.reshape(th, tw, 4)
        b.clear_pose()
        # A 2 px dark line between the tiles.
        for c in range(1, cols):
            sheet[:, c * tw - 1:c * tw + 1, :3] = 0.1
        for r in range(1, rows):
            sheet[r * th - 1:r * th + 1, :, :3] = 0.1
        out = bpy.data.images.new(name + "_sheet", cols * tw, rows * th, alpha=False)
        out.pixels.foreach_set(sheet.ravel())
        out.filepath_raw = PREVIEW_CLIPS[name]
        out.file_format = "PNG"
        out.save()
        bpy.data.images.remove(out)
        print("wrote", PREVIEW_CLIPS[name])
    os.remove(tmp)
    for o in (ground, cam):
        bpy.data.objects.remove(o)
    bpy.data.meshes.remove(gm)
    bpy.data.cameras.remove(cam_data)
    bpy.data.materials.remove(sand)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = Builder()
    b.build_all()
    problems, m = check(b)
    print("arms:", json.dumps(m, indent=1))
    cp, cm = clip_checks(b)
    print("clips:", json.dumps(cm, indent=1))
    problems += cp
    if problems:
        raise RuntimeError("the arms fail their checks:\n  " + "\n  ".join(problems))
    if os.environ.get("PXE_SKIP_EXPORT"):
        return
    export()
    # The glb the clips are added to: its rest pose must come back unchanged.
    with open(OUT, "rb") as fh:
        before = fh.read()
    add_clips(OUT, b)
    gp, gm = check_glb(OUT, m["triangles"])
    with open(OUT, "rb") as fh:
        ap, am = check_glb_clips(fh.read(), before, b)
    gp += ap
    gm.update(am)
    print("arms.glb:", json.dumps(gm))
    if gp:
        raise RuntimeError("arms.glb fails its checks:\n  " + "\n  ".join(gp))
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_previews(b)


if __name__ == "__main__" and bpy.app.background:
    main()
