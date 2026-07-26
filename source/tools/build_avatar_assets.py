#!/usr/bin/env python3
"""Build the native CharacterStudio catalog and animated modular avatar assets.

The converter reads the upstream CharacterStudio manifests directly, converts every
VRM trait option, preserves all texture/color variants, and uses Assimp to sample
the supplied Mixamo walking/running FBX clips. Skinning is baked into five poses
(idle, walk A/B, run A/B) so the native runtime stays small and Emscripten-friendly.
"""

from __future__ import annotations

import argparse
import bisect
import copy
import json
import math
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path
from typing import Any, Iterable
from urllib.parse import unquote

ASSET_MAGIC = b"M3A2"
ASSET_VERSION = 2
CATALOG_MAGIC = b"M3C1"
CATALOG_VERSION = 1
POSE_NAMES = ("idle", "walkA", "walkB", "runA", "runB", "wave", "cheer", "dance")

COMPONENT_FORMATS = {
    5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
    5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4),
}
COMPONENT_COUNTS = {
    "SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
    "MAT2": 4, "MAT3": 9, "MAT4": 16,
}

SPANISH_GROUP_NAMES = {
    "head": "CABELLO", "body": "CUERPO", "eyes": "OJOS", "outer": "ABRIGOS",
    "chest": "TORSO", "legs": "PIERNAS", "feet": "CALZADO", "accessories": "ACCESORIOS",
}

SPANISH_OPTIONS = {
    "head": {
        "buns": "Moños", "curledbangs": "Flequillo rizado", "dreds": "Rastas",
        "longspike": "Punta larga", "ponytail": "Cola de caballo", "short": "Corto",
        "shortband": "Corto con banda", "shortcurve": "Corto curvo", "straight": "Lacio",
        "swept": "Peinado lateral", "hair1": "Cabello 1", "hair2": "Cabello 2",
        "hair3": "Cabello 3", "hair4": "Cabello 4", "hair5": "Cabello 5",
        "braided": "Trenzado", "hairshort": "Cabello corto", "straightdown": "Lacio largo",
    },
    "body": {"0": "Cuerpo atlético"},
    "eyes": {"regulareyes": "Ojos normales"},
    "outer": {"halfjacket": "Media chaqueta", "jacket": "Chaqueta", "shortjacket": "Chaqueta corta",
              "longjacket": "Chaqueta larga", "robe": "Túnica"},
    "chest": {
        "croptop": "Top corto", "hoodie": "Sudadera", "jumpsuittop": "Parte superior de mono",
        "lightshirt": "Camisa ligera", "shirt": "Camisa", "sweater": "Suéter",
        "tanktop": "Camiseta sin mangas", "tuckedshirt": "Camisa fajada", "dress": "Vestido",
        "fulljacket": "Chaqueta completa", "combatvest": "Chaleco de combate", "logotee": "Camiseta con logotipo",
        "simpleshirt": "Camisa sencilla", "slottedshirt": "Camisa ranurada", "dressjacket": "Chaqueta de vestir",
        "dressrobe": "Túnica de gala", "fancysuit": "Traje elegante", "streetware": "Ropa urbana",
    },
    "legs": {
        "cargopants": "Pantalones cargo", "doublebeltpants": "Pantalones de doble cinturón",
        "jumpsuitpants": "Pantalones de mono", "abovekneeshorts": "Pantalones cortos sobre la rodilla",
        "casualshorts": "Pantalones cortos casuales", "skirt": "Falda", "sportshorts": "Pantalones deportivos",
        "sportshortswithanklebands": "Pantalones deportivos con tobilleras", "waistshorts": "Pantalones cortos de cintura",
        "cargoshorts": "Pantalones cargo cortos", "rolleduppants": "Pantalones remangados", "shorts": "Pantalones cortos",
        "shortsandsweats": "Pantalones cortos y deportivos", "streetpants": "Pantalones urbanos",
        "suitpants": "Pantalones de traje", "tightjeans": "Vaqueros ajustados", "workingpants": "Pantalones de trabajo",
    },
    "feet": {
        "designertallboots": "Botas altas de diseñador", "tallboots": "Botas altas", "designershoes": "Zapatos de diseñador",
        "dressboots2": "Botas de vestir 2", "dressboots": "Botas de vestir", "hightop": "Tenis altos",
        "shortboots": "Botas cortas", "sneakers": "Tenis", "tennisshoes": "Zapatillas", "thinshoe": "Zapato ligero",
        "boots": "Botas", "dressshoes": "Zapatos de vestir", "fantasyboots": "Botas de fantasía",
        "hikingboots": "Botas de senderismo", "runningshoes": "Calzado para correr", "shroudedboots": "Botas cubiertas",
        "simpleshoes": "Zapatos sencillos", "sinchboots": "Botas ceñidas", "strapedboots": "Botas con correas",
    },
    "accessories": {
        "hipbelt": "Cinturón de cadera", "legstrapandcompress": "Correa y compresa de pierna",
        "bandolier": "Bandolera", "beltandbag": "Cinturón y bolsa", "chestrig": "Arnés de pecho",
        "combatgloves": "Guantes de combate", "fingergloves": "Guantes sin dedos",
        "legbandages": "Vendajes de pierna", "ninjagloves": "Guantes ninja",
    },
}

SPANISH_COLORS = {
    "Emerald":"Esmeralda", "Sacramento":"Sacramento", "Kelly":"Verde Kelly", "Lime":"Lima",
    "Clay":"Arcilla", "Latte":"Café con leche", "Copper":"Cobre", "Dark Grey":"Gris oscuro",
    "Pink":"Rosa", "Red":"Rojo", "Scarlet":"Escarlata", "Maroon":"Granate", "Sangria":"Sangría",
    "Lavender":"Lavanda", "Lilac":"Lila", "Violet":"Violeta", "Dark Purple":"Púrpura oscuro",
    "Periwinkle":"Bígaro", "Purple":"Púrpura", "Mauve":"Malva", "Eggplant":"Berenjena",
    "Amethyst":"Amatista", "Iris":"Iris", "Heather":"Brezo", "Blue":"Azul", "Navy":"Azul marino",
    "Aegean":"Egeo", "Cerulean":"Cerúleo", "Berry":"Baya", "Indigo":"Índigo",
    "Light Cyan":"Cian claro", "Dark Cyan":"Cian oscuro", "Teal":"Verde azulado", "Arctic":"Ártico",
    "Sky":"Cielo", "Peanut":"Cacahuate", "Coffee":"Café", "Mocha":"Moca", "Syrup":"Jarabe",
    "Chocolate":"Chocolate", "Dark Wood":"Madera oscura", "Hazelnut":"Avellana",
    "Hazel Wood":"Madera avellana", "Fawn":"Beige", "Tan":"Canela", "Sand":"Arena",
    "Yellow":"Amarillo", "Amber":"Ámbar", "Orange":"Naranja", "Black":"Negro", "Leaf":"Hoja",
    "Pale Blue":"Azul pálido", "Light Orange":"Naranja claro", "Fire":"Fuego",
}


def spanish_option_name(group_id: str, option_id: str, fallback: str) -> str:
    return SPANISH_OPTIONS.get(group_id, {}).get(option_id, fallback)


def spanish_variant_name(name: str) -> str:
    if name in SPANISH_COLORS:
        return SPANISH_COLORS[name]
    if name.endswith(" Eyes"):
        color = name[:-5]
        return "Ojos " + SPANISH_COLORS.get(color, color).lower()
    replacements = {
        "Original Skin":"Piel original", "Original Arm Sleeve":"Mangas originales",
        "Original Knee High":"Medias originales a la rodilla", "Original Thigh High":"Medias originales al muslo",
    }
    if name in replacements:
        return replacements[name]
    for tone in range(1, 10):
        name = name.replace(f"Tone{tone} Skin", f"Piel tono {tone}")
        name = name.replace(f"Tone{tone} Arm Sleeve", f"Mangas tono {tone}")
        name = name.replace(f"Tone{tone} Knee High", f"Medias a la rodilla tono {tone}")
        name = name.replace(f"Tone{tone} Thigh High", f"Medias al muslo tono {tone}")
    if "_" in name and name.rsplit("_", 1)[-1].isdigit():
        return f"Variante {int(name.rsplit('_', 1)[-1]) + 1}"
    return name

MIXAMO_TO_VRM = {
    "Hips": "hips", "Spine": "spine", "Spine1": "chest", "Spine2": "upperChest",
    "Neck": "neck", "Head": "head",
    "LeftShoulder": "leftShoulder", "LeftArm": "leftUpperArm",
    "LeftForeArm": "leftLowerArm", "LeftHand": "leftHand",
    "RightShoulder": "rightShoulder", "RightArm": "rightUpperArm",
    "RightForeArm": "rightLowerArm", "RightHand": "rightHand",
    "LeftUpLeg": "leftUpperLeg", "LeftLeg": "leftLowerLeg",
    "LeftFoot": "leftFoot", "LeftToeBase": "leftToes",
    "RightUpLeg": "rightUpperLeg", "RightLeg": "rightLowerLeg",
    "RightFoot": "rightFoot", "RightToeBase": "rightToes",
}
for side in ("Left", "Right"):
    prefix = "left" if side == "Left" else "right"
    for source, target in (
        ("Thumb1", "ThumbMetacarpal"), ("Thumb2", "ThumbProximal"), ("Thumb3", "ThumbDistal"),
        ("Index1", "IndexProximal"), ("Index2", "IndexIntermediate"), ("Index3", "IndexDistal"),
        ("Middle1", "MiddleProximal"), ("Middle2", "MiddleIntermediate"), ("Middle3", "MiddleDistal"),
        ("Ring1", "RingProximal"), ("Ring2", "RingIntermediate"), ("Ring3", "RingDistal"),
        ("Pinky1", "LittleProximal"), ("Pinky2", "LittleIntermediate"), ("Pinky3", "LittleDistal"),
    ):
        MIXAMO_TO_VRM[f"{side}Hand{source}"] = f"{prefix}{target}"


def identity() -> list[float]:
    return [1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0]


def mat_mul(a: list[float], b: list[float]) -> list[float]:
    out = [0.0] * 16
    for col in range(4):
        for row in range(4):
            out[col * 4 + row] = sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
    return out


def quat_normalize(q: Iterable[float]) -> tuple[float, float, float, float]:
    values = tuple(float(v) for v in q)
    length = math.sqrt(sum(v * v for v in values))
    if length < 1.0e-10:
        return (0.0, 0.0, 0.0, 1.0)
    return tuple(v / length for v in values)  # type: ignore[return-value]


def quat_mul(a: Iterable[float], b: Iterable[float]) -> tuple[float, float, float, float]:
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return quat_normalize((
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ))


def quat_inverse(q: Iterable[float]) -> tuple[float, float, float, float]:
    x, y, z, w = quat_normalize(q)
    return (-x, -y, -z, w)


def quat_slerp(a: Iterable[float], b: Iterable[float], t: float) -> tuple[float, float, float, float]:
    qa = quat_normalize(a)
    qb = quat_normalize(b)
    dot = sum(x * y for x, y in zip(qa, qb))
    if dot < 0.0:
        qb = tuple(-v for v in qb)
        dot = -dot
    if dot > 0.9995:
        return quat_normalize(tuple(x + (y - x) * t for x, y in zip(qa, qb)))
    theta = math.acos(max(-1.0, min(1.0, dot)))
    sin_theta = math.sin(theta)
    wa = math.sin((1.0 - t) * theta) / sin_theta
    wb = math.sin(t * theta) / sin_theta
    return quat_normalize(tuple(x * wa + y * wb for x, y in zip(qa, qb)))


def axis_angle(axis: tuple[float, float, float], radians: float) -> tuple[float, float, float, float]:
    length = math.sqrt(sum(v * v for v in axis))
    half = radians * 0.5
    scale = math.sin(half) / max(length, 1.0e-8)
    return quat_normalize((axis[0] * scale, axis[1] * scale, axis[2] * scale, math.cos(half)))


def matrix_from_trs(translation: Iterable[float], rotation: Iterable[float], scale: Iterable[float]) -> list[float]:
    tx, ty, tz = translation
    sx, sy, sz = scale
    x, y, z, w = quat_normalize(rotation)
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        (1 - 2 * (yy + zz)) * sx, (2 * (xy + wz)) * sx, (2 * (xz - wy)) * sx, 0.0,
        (2 * (xy - wz)) * sy, (1 - 2 * (xx + zz)) * sy, (2 * (yz + wx)) * sy, 0.0,
        (2 * (xz + wy)) * sz, (2 * (yz - wx)) * sz, (1 - 2 * (xx + yy)) * sz, 0.0,
        float(tx), float(ty), float(tz), 1.0,
    ]


def matrix_quaternion(m: list[float]) -> tuple[float, float, float, float]:
    sx = math.sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]) or 1.0
    sy = math.sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]) or 1.0
    sz = math.sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]) or 1.0
    r00, r01, r02 = m[0] / sx, m[4] / sy, m[8] / sz
    r10, r11, r12 = m[1] / sx, m[5] / sy, m[9] / sz
    r20, r21, r22 = m[2] / sx, m[6] / sy, m[10] / sz
    trace = r00 + r11 + r22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        q = ((r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s, 0.25 * s)
    elif r00 > r11 and r00 > r22:
        s = math.sqrt(1.0 + r00 - r11 - r22) * 2.0
        q = (0.25 * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s)
    elif r11 > r22:
        s = math.sqrt(1.0 + r11 - r00 - r22) * 2.0
        q = ((r01 + r10) / s, 0.25 * s, (r12 + r21) / s, (r02 - r20) / s)
    else:
        s = math.sqrt(1.0 + r22 - r00 - r11) * 2.0
        q = ((r02 + r20) / s, (r12 + r21) / s, 0.25 * s, (r10 - r01) / s)
    return quat_normalize(q)


def transform_point(m: list[float], p: Iterable[float]) -> tuple[float, float, float]:
    x, y, z = p
    return (m[0] * x + m[4] * y + m[8] * z + m[12],
            m[1] * x + m[5] * y + m[9] * z + m[13],
            m[2] * x + m[6] * y + m[10] * z + m[14])


def transform_direction(m: list[float], n: Iterable[float]) -> tuple[float, float, float]:
    x, y, z = n
    out = (m[0] * x + m[4] * y + m[8] * z,
           m[1] * x + m[5] * y + m[9] * z,
           m[2] * x + m[6] * y + m[10] * z)
    length = math.sqrt(sum(v * v for v in out))
    return (0.0, 1.0, 0.0) if length < 1.0e-8 else tuple(v / length for v in out)  # type: ignore[return-value]


def node_trs(node: dict[str, Any]) -> tuple[list[float], tuple[float, float, float, float], list[float]]:
    return ([float(v) for v in node.get("translation", [0, 0, 0])],
            quat_normalize(node.get("rotation", [0, 0, 0, 1])),
            [float(v) for v in node.get("scale", [1, 1, 1])])


def node_matrix(node: dict[str, Any]) -> list[float]:
    if "matrix" in node:
        return [float(v) for v in node["matrix"]]
    return matrix_from_trs(*node_trs(node))


def scene_globals(doc: dict[str, Any], local_matrices: list[list[float]] | None = None) -> list[list[float]]:
    nodes = doc.get("nodes", [])
    locals_ = local_matrices or [node_matrix(node) for node in nodes]
    globals_ = [identity() for _ in nodes]
    visited: set[int] = set()

    def visit(index: int, parent: list[float]) -> None:
        globals_[index] = mat_mul(parent, locals_[index])
        visited.add(index)
        for child in nodes[index].get("children", []):
            visit(int(child), globals_[index])

    scenes = doc.get("scenes", [{"nodes": list(range(len(nodes)))}])
    for root in scenes[int(doc.get("scene", 0))].get("nodes", []):
        visit(int(root), identity())
    for index in range(len(nodes)):
        if index not in visited:
            visit(index, identity())
    return globals_


def parse_glb(path: Path) -> tuple[dict[str, Any], list[bytes]]:
    data = path.read_bytes()
    magic, version, declared_length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or declared_length > len(data):
        raise ValueError(f"{path}: expected a glTF 2.0 binary")
    doc: dict[str, Any] | None = None
    binary = b""
    offset = 12
    while offset + 8 <= declared_length:
        length, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        payload = data[offset:offset + length]
        offset += length
        if kind == 0x4E4F534A:
            doc = json.loads(payload.rstrip(b"\x00 \t\r\n").decode())
        elif kind == 0x004E4942:
            binary = payload
    if doc is None:
        raise ValueError(f"{path}: no JSON chunk")
    return doc, [binary]


def parse_gltf(path: Path) -> tuple[dict[str, Any], list[bytes]]:
    doc = json.loads(path.read_text(encoding="utf-8"))
    buffers: list[bytes] = []
    for buffer in doc.get("buffers", []):
        uri = unquote(buffer["uri"])
        buffers.append((path.parent / uri).read_bytes())
    return doc, buffers


def normalized_value(value: int | float, component_type: int) -> float:
    divisors = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0, 5125: 4294967295.0}
    value = float(value) / divisors.get(component_type, 1.0)
    return max(-1.0, value) if component_type in (5120, 5122) else value


def read_accessor(doc: dict[str, Any], buffers: list[bytes], accessor_index: int) -> list[tuple[Any, ...]]:
    accessor = doc["accessors"][accessor_index]
    if "sparse" in accessor:
        raise ValueError("Sparse accessors are not supported")
    view = doc["bufferViews"][accessor["bufferView"]]
    data = buffers[int(view.get("buffer", 0))]
    component_type = int(accessor["componentType"])
    fmt, component_size = COMPONENT_FORMATS[component_type]
    component_count = COMPONENT_COUNTS[accessor["type"]]
    element_size = component_size * component_count
    stride = int(view.get("byteStride", element_size))
    start = int(view.get("byteOffset", 0)) + int(accessor.get("byteOffset", 0))
    unpack_fmt = "<" + fmt * component_count
    result = []
    for i in range(int(accessor["count"])):
        value = struct.unpack_from(unpack_fmt, data, start + i * stride)
        if accessor.get("normalized", False):
            value = tuple(normalized_value(v, component_type) for v in value)
        result.append(value)
    return result


class AnimationClip:
    def __init__(self, doc: dict[str, Any], buffers: list[bytes]):
        self.doc = doc
        self.buffers = buffers
        animation = doc["animations"][0]
        self.channels: dict[tuple[int, str], tuple[list[float], list[tuple[Any, ...]], str]] = {}
        self.duration = 0.0
        for channel in animation["channels"]:
            sampler = animation["samplers"][channel["sampler"]]
            times = [float(v[0]) for v in read_accessor(doc, buffers, sampler["input"])]
            values = read_accessor(doc, buffers, sampler["output"])
            self.duration = max(self.duration, times[-1] if times else 0.0)
            target = channel["target"]
            self.channels[(int(target["node"]), target["path"])] = (times, values, sampler.get("interpolation", "LINEAR"))
        self.rest_globals = scene_globals(doc)

    def sample_value(self, node: int, path: str, time: float, default: Iterable[float]) -> tuple[float, ...]:
        track = self.channels.get((node, path))
        if not track:
            return tuple(float(v) for v in default)
        times, values, interpolation = track
        if not times:
            return tuple(float(v) for v in default)
        time = max(times[0], min(time, times[-1]))
        upper = bisect.bisect_right(times, time)
        if upper <= 0:
            return tuple(float(v) for v in values[0])
        if upper >= len(times):
            return tuple(float(v) for v in values[-1])
        lower = upper - 1
        if interpolation == "STEP" or times[upper] <= times[lower]:
            return tuple(float(v) for v in values[lower])
        alpha = (time - times[lower]) / (times[upper] - times[lower])
        if path == "rotation":
            return quat_slerp(values[lower], values[upper], alpha)
        return tuple(float(a) + (float(b) - float(a)) * alpha for a, b in zip(values[lower], values[upper]))

    def globals_at(self, normalized_time: float) -> list[list[float]]:
        time = self.duration * normalized_time
        locals_: list[list[float]] = []
        for index, node in enumerate(self.doc.get("nodes", [])):
            if "matrix" in node and not any((index, path) in self.channels for path in ("translation", "rotation", "scale")):
                locals_.append(node_matrix(node))
                continue
            translation, rotation, scale = node_trs(node)
            translation = list(self.sample_value(index, "translation", time, translation))
            rotation = self.sample_value(index, "rotation", time, rotation)
            scale = list(self.sample_value(index, "scale", time, scale))
            locals_.append(matrix_from_trs(translation, rotation, scale))
        return scene_globals(self.doc, locals_)

    def humanoid_sample(self, normalized_time: float) -> dict[str, Any]:
        animated = self.globals_at(normalized_time)
        node_by_name = {node.get("name", ""): i for i, node in enumerate(self.doc.get("nodes", []))}
        rotations: dict[str, tuple[float, float, float, float]] = {}
        hips_ratio = 0.0
        for mixamo_name, vrm_name in MIXAMO_TO_VRM.items():
            source_name = f"mixamorig:{mixamo_name}"
            node_index = node_by_name.get(source_name)
            if node_index is None:
                continue
            rest_q = matrix_quaternion(self.rest_globals[node_index])
            anim_q = matrix_quaternion(animated[node_index])
            rotations[vrm_name] = quat_mul(anim_q, quat_inverse(rest_q))
            if mixamo_name == "Hips":
                rest_y = self.rest_globals[node_index][13]
                anim_y = animated[node_index][13]
                hips_ratio = (anim_y - rest_y) / max(abs(rest_y), 1.0)
        return {"rotations": rotations, "hips_ratio": hips_ratio}


def build_animation_samples(assimp: Path, animation_root: Path, pose_animation_root: Path,
                            work_dir: Path) -> dict[str, dict[str, Any]]:
    samples: dict[str, dict[str, Any]] = {}

    def load_clip(key: str, root: Path, filename: str) -> AnimationClip:
        output = work_dir / f"{key}.gltf"
        command = [str(assimp), "export", str(root / filename), str(output), "-f", "gltf2"]
        result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Assimp could not convert {filename}: {result.stderr.strip()}")
        doc, buffers = parse_gltf(output)
        clip = AnimationClip(doc, buffers)
        print(f"Animation {filename}: {clip.duration:.3f}s, {len(clip.channels)} channels")
        return clip

    for clip_name, filename in (("walk", "Walking.fbx"), ("run", "Running.fbx")):
        clip = load_clip(clip_name, animation_root, filename)
        samples[f"{clip_name}A"] = clip.humanoid_sample(0.20)
        samples[f"{clip_name}B"] = clip.humanoid_sample(0.70)

    for key, filename, sample_time in (
        ("wave", "Waving.fbx", 0.55),
        ("cheer", "Cheering.fbx", 0.48),
        ("dance", "Dancing.fbx", 0.32),
    ):
        clip = load_clip(key, pose_animation_root, filename)
        samples[key] = clip.humanoid_sample(sample_time)
    return samples


def human_bones(doc: dict[str, Any]) -> dict[str, int]:
    source = doc.get("extensions", {}).get("VRMC_vrm", {}).get("humanoid", {}).get("humanBones", {})
    return {name: int(value["node"]) for name, value in source.items()}


def relaxed_globals(doc: dict[str, Any]) -> list[list[float]]:
    bones = human_bones(doc)
    locals_ = [node_matrix(node) for node in doc.get("nodes", [])]
    for name, radians in (("leftUpperArm", -67.0), ("rightUpperArm", 67.0)):
        node_index = bones.get(name)
        if node_index is None:
            continue
        translation, rotation, scale = node_trs(doc["nodes"][node_index])
        rotation = quat_mul(rotation, axis_angle((0, 0, 1), math.radians(radians)))
        locals_[node_index] = matrix_from_trs(translation, rotation, scale)
    return scene_globals(doc, locals_)


def retarget_globals(doc: dict[str, Any], sample: dict[str, Any]) -> list[list[float]]:
    nodes = doc.get("nodes", [])
    bones = human_bones(doc)
    node_to_bone = {node: name for name, node in bones.items()}
    rest_globals = scene_globals(doc)
    output = [identity() for _ in nodes]
    visited: set[int] = set()
    target_hips_height = abs(rest_globals[bones["hips"]][13]) if "hips" in bones else 1.0

    def visit(index: int, parent_global: list[float]) -> None:
        translation, local_rotation, scale = node_trs(nodes[index])
        bone_name = node_to_bone.get(index)
        delta = sample["rotations"].get(bone_name) if bone_name else None
        if delta is not None:
            desired_global = quat_mul(delta, matrix_quaternion(rest_globals[index]))
            local_rotation = quat_mul(quat_inverse(matrix_quaternion(parent_global)), desired_global)
            if bone_name == "hips":
                translation[1] += float(sample.get("hips_ratio", 0.0)) * target_hips_height
        local = matrix_from_trs(translation, local_rotation, scale)
        output[index] = mat_mul(parent_global, local)
        visited.add(index)
        for child in nodes[index].get("children", []):
            visit(int(child), output[index])

    scenes = doc.get("scenes", [{"nodes": list(range(len(nodes)))}])
    for root in scenes[int(doc.get("scene", 0))].get("nodes", []):
        visit(int(root), identity())
    for index in range(len(nodes)):
        if index not in visited:
            visit(index, identity())
    return output


def embedded_image(doc: dict[str, Any], buffers: list[bytes], material: dict[str, Any]) -> bytes:
    texture_info = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if not texture_info:
        return b""
    source = int(doc["textures"][int(texture_info["index"])]["source"])
    image = doc["images"][source]
    view = doc["bufferViews"][int(image["bufferView"])]
    data = buffers[int(view.get("buffer", 0))]
    start = int(view.get("byteOffset", 0))
    return data[start:start + int(view["byteLength"])]


def skin_vertex(position: tuple[Any, ...], normal: tuple[Any, ...], joints: tuple[Any, ...] | None,
                weights: tuple[Any, ...] | None, skin_matrices: list[list[float]] | None,
                mesh_matrix: list[float]) -> tuple[float, float, float, float, float, float]:
    if skin_matrices is None or joints is None or weights is None:
        p = transform_point(mesh_matrix, position[:3])
        n = transform_direction(mesh_matrix, normal[:3])
        return (*p, *n)
    p = [0.0, 0.0, 0.0]
    n = [0.0, 0.0, 0.0]
    total = 0.0
    for joint, weight in zip(joints, weights):
        weight = float(weight)
        if weight <= 0.0:
            continue
        matrix = skin_matrices[int(joint)]
        tp = transform_point(matrix, position[:3])
        tn = transform_direction(matrix, normal[:3])
        for component in range(3):
            p[component] += tp[component] * weight
            n[component] += tn[component] * weight
        total += weight
    if total <= 1.0e-6:
        return (*transform_point(mesh_matrix, position[:3]), *transform_direction(mesh_matrix, normal[:3]))
    p = [value / total for value in p]
    length = math.sqrt(sum(value * value for value in n))
    n = [value / max(length, 1.0e-8) for value in n]
    return (*transform_point(mesh_matrix, p), *transform_direction(mesh_matrix, n))


def extract_vrm(path: Path, label: str, animation_samples: dict[str, dict[str, Any]], mesh_slot_base: int = 0) -> tuple[list[dict[str, Any]], int]:
    doc, buffers = parse_glb(path)
    pose_globals = {
        "idle": relaxed_globals(doc),
        "walkA": retarget_globals(doc, animation_samples["walkA"]),
        "walkB": retarget_globals(doc, animation_samples["walkB"]),
        "runA": retarget_globals(doc, animation_samples["runA"]),
        "runB": retarget_globals(doc, animation_samples["runB"]),
        "wave": retarget_globals(doc, animation_samples["wave"]),
        "cheer": retarget_globals(doc, animation_samples["cheer"]),
        "dance": retarget_globals(doc, animation_samples["dance"]),
    }
    rest_globals = scene_globals(doc)
    primitives: list[dict[str, Any]] = []
    mesh_slot = mesh_slot_base

    for node_index, node in enumerate(doc.get("nodes", [])):
        if "mesh" not in node:
            continue
        mesh = doc["meshes"][int(node["mesh"])]
        mesh_matrix = rest_globals[node_index]
        pose_skin_matrices: dict[str, list[list[float]] | None] = {name: None for name in POSE_NAMES}
        if "skin" in node:
            skin = doc["skins"][int(node["skin"])]
            inverse_bind = read_accessor(doc, buffers, int(skin["inverseBindMatrices"]))
            for pose_name in POSE_NAMES:
                pose_skin_matrices[pose_name] = [
                    mat_mul(pose_globals[pose_name][int(joint_node)], list(inverse_bind[joint_index]))
                    for joint_index, joint_node in enumerate(skin["joints"])
                ]

        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            if int(primitive.get("mode", 4)) != 4:
                continue
            attrs = primitive.get("attributes", {})
            positions = read_accessor(doc, buffers, int(attrs["POSITION"]))
            normals = read_accessor(doc, buffers, int(attrs["NORMAL"])) if "NORMAL" in attrs else [(0, 1, 0)] * len(positions)
            uvs = read_accessor(doc, buffers, int(attrs["TEXCOORD_0"])) if "TEXCOORD_0" in attrs else [(0, 0)] * len(positions)
            joints = read_accessor(doc, buffers, int(attrs["JOINTS_0"])) if "JOINTS_0" in attrs else None
            weights = read_accessor(doc, buffers, int(attrs["WEIGHTS_0"])) if "WEIGHTS_0" in attrs else None
            indices = [int(v[0]) for v in read_accessor(doc, buffers, int(primitive["indices"]))] if "indices" in primitive else list(range(len(positions)))
            material_index = int(primitive.get("material", -1))
            material = doc.get("materials", [])[material_index] if material_index >= 0 else {}
            base_color = [float(v) for v in material.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])]
            flags = (1 if material.get("doubleSided", False) else 0)
            flags |= 2 if material.get("alphaMode") == "BLEND" else 0
            flags |= 4 if material.get("alphaMode") == "MASK" else 0

            vertex_data: list[float] = []
            for vertex_index, (position, normal, uv) in enumerate(zip(positions, normals, uvs)):
                pose_values: dict[str, tuple[float, ...]] = {}
                for pose_name in POSE_NAMES:
                    pose_values[pose_name] = skin_vertex(
                        position, normal,
                        joints[vertex_index] if joints else None,
                        weights[vertex_index] if weights else None,
                        pose_skin_matrices[pose_name], mesh_matrix,
                    )
                idle = pose_values["idle"]
                vertex_data.extend((*idle[:6], float(uv[0]), float(uv[1])))
                for pose_name in POSE_NAMES[1:]:
                    vertex_data.extend(pose_values[pose_name][:6])

            primitives.append({
                "label": f"{label}/{mesh.get('name', 'mesh')}:{primitive_index}",
                "mesh_slot": mesh_slot,
                "vertices": vertex_data,
                "indices": indices,
                "image": embedded_image(doc, buffers, material),
                "flags": flags,
                "base_color": base_color,
                "alpha_cutoff": float(material.get("alphaCutoff", 0.5)),
            })
        mesh_slot += 1
    if not primitives:
        raise ValueError(f"{path}: no triangle primitives")
    return primitives, mesh_slot


def write_asset(path: Path, primitives: list[dict[str, Any]]) -> tuple[list[float], list[float]]:
    path.parent.mkdir(parents=True, exist_ok=True)
    bounds_min = [float("inf")] * 3
    bounds_max = [float("-inf")] * 3
    with path.open("wb") as out:
        out.write(struct.pack("<4sIII", ASSET_MAGIC, ASSET_VERSION, len(primitives), 0))
        for primitive in primitives:
            vertices = primitive["vertices"]
            indices = primitive["indices"]
            image = primitive["image"]
            label = primitive["label"].encode()
            if len(vertices) % 50:
                raise ValueError("Internal vertex packing error")
            vertex_count = len(vertices) // 50
            for vertex in range(vertex_count):
                base = vertex * 50
                for component in range(3):
                    bounds_min[component] = min(bounds_min[component], vertices[base + component])
                    bounds_max[component] = max(bounds_max[component], vertices[base + component])
            out.write(struct.pack(
                "<IIIII4ffI", vertex_count, len(indices), len(image), primitive["flags"],
                primitive["mesh_slot"], *primitive["base_color"], primitive["alpha_cutoff"], len(label)))
            out.write(label)
            out.write(struct.pack(f"<{len(vertices)}f", *vertices))
            out.write(struct.pack(f"<{len(indices)}I", *indices))
            out.write(image)
    return bounds_min, bounds_max


def sanitize(value: str) -> str:
    return "".join(character if character.isalnum() or character in "-_" else "_" for character in value)


def parse_hex_color(value: str) -> tuple[float, float, float]:
    value = value.lstrip("#")
    if len(value) == 3:
        value = "".join(character * 2 for character in value)
    return tuple(int(value[index:index + 2], 16) / 255.0 for index in (0, 2, 4))  # type: ignore[return-value]


def normalize_paths(directory: Any) -> list[str]:
    return [str(path) for path in (directory if isinstance(directory, list) else [directory]) if path]


def build_catalog(config: dict[str, Any], asset_root: Path, output_dir: Path,
                  animation_samples: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    catalog: list[dict[str, Any]] = []
    converted_manifests: dict[tuple[str, str], dict[str, Any]] = {}

    for character_config in config["characters"]:
        character_id = str(character_config["id"])
        family = str(character_config.get("family", character_id))
        manifest_path = (asset_root / character_config["manifest"]).resolve()
        cache_key = (str(manifest_path), family)

        if cache_key not in converted_manifests:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            character_dir = manifest_path.parent
            source_folder = character_dir.name
            texture_groups = {group["trait"]: group for group in manifest.get("textureCollections", [])}
            color_groups = {group["trait"]: group for group in manifest.get("colorCollections", [])}
            required = set(manifest.get("requiredTraits", []))
            groups_template: list[dict[str, Any]] = []
            body_bounds: tuple[list[float], list[float]] | None = None

            for group in manifest.get("traits", []):
                group_id = str(group["trait"])
                options_out: list[dict[str, Any]] = []
                for option in group.get("collection", []):
                    option_id = str(option["id"])
                    rel_asset = Path("traits") / family / sanitize(group_id) / f"{sanitize(option_id)}.m3a"
                    model_paths = normalize_paths(option["directory"])
                    primitives: list[dict[str, Any]] = []
                    mesh_slot = 0
                    for model_path in model_paths:
                        extracted, mesh_slot = extract_vrm(
                            character_dir / model_path,
                            f"{group_id}:{option.get('name', option_id)}",
                            animation_samples, mesh_slot)
                        primitives.extend(extracted)
                    bounds = write_asset(output_dir / rel_asset, primitives)
                    if group_id == "body" and body_bounds is None:
                        body_bounds = bounds

                    variant_kind = 0
                    variants: list[dict[str, Any]] = []
                    collection_name = option.get("textureCollection") or option.get("colorCollection")
                    if option.get("textureCollection") and collection_name in texture_groups:
                        variant_kind = 1
                        for variant in texture_groups[collection_name].get("collection", []):
                            paths = [str((Path(source_folder) / path).as_posix()) for path in normalize_paths(variant.get("directory"))]
                            variants.append({"id": str(variant.get("id", "")), "name": spanish_variant_name(str(variant.get("name", variant.get("id", "Textura")))), "paths": paths, "colors": []})
                    elif option.get("colorCollection") and collection_name in color_groups:
                        variant_kind = 2
                        for variant in color_groups[collection_name].get("collection", []):
                            colors = [parse_hex_color(value) for value in normalize_paths(variant.get("value"))]
                            variants.append({"id": str(variant.get("id", "")), "name": spanish_variant_name(str(variant.get("name", variant.get("id", "Color")))), "paths": [], "colors": colors})
                    options_out.append({
                        "id": option_id,
                        "name": spanish_option_name(group_id, option_id, str(option.get("name", option_id))),
                        "asset": rel_asset.as_posix(),
                        "thumbnail": (Path(source_folder) / str(option.get("thumbnail", ""))).as_posix(),
                        "types": [str(value) for value in (option.get("type", []) if isinstance(option.get("type", []), list) else [option.get("type")]) if value],
                        "variant_kind": variant_kind,
                        "default_variant": 0 if variants else -1,
                        "variants": variants,
                    })
                    print(f"[{family}] {group_id}/{option_id}: {len(primitives)} primitives")
                groups_template.append({
                    "id": group_id,
                    "name": SPANISH_GROUP_NAMES.get(group_id, str(group.get("name", group_id))).upper(),
                    "required": bool(group_id in required or group.get("required")),
                    "default": 0 if (group_id in required or group.get("required")) and options_out else -1,
                    "options": options_out,
                })

            if body_bounds is None:
                body_bounds = ([-0.5, 0.0, -0.5], [0.5, 1.7, 0.5])
            converted_manifests[cache_key] = {
                "groups": groups_template,
                "bounds": body_bounds,
                "type_restrictions": manifest.get("typeRestrictions", {}),
                "trait_restrictions": manifest.get("traitRestrictions", {}),
            }

        template = converted_manifests[cache_key]
        groups_out = copy.deepcopy(template["groups"])
        defaults = {str(key): str(value) for key, value in character_config.get("traits", {}).items()}
        variant_defaults = character_config.get("variants", {})
        for group in groups_out:
            default_id = defaults.get(group["id"])
            default_index = next((i for i, option in enumerate(group["options"]) if option["id"] == default_id), None)
            if default_index is None:
                default_index = 0 if group["required"] and group["options"] else -1
            group["default"] = default_index
            if default_index >= 0:
                selected_option = group["options"][default_index]
                requested_variant = variant_defaults.get(group["id"])
                if selected_option["variants"] and requested_variant is not None:
                    if isinstance(requested_variant, int):
                        selected_option["default_variant"] = requested_variant % len(selected_option["variants"])
                    else:
                        selected_option["default_variant"] = next(
                            (i for i, variant in enumerate(selected_option["variants"]) if variant["id"] == str(requested_variant)), 0)

        bounds_min, bounds_max = template["bounds"]
        catalog.append({
            "id": character_id,
            "name": character_config["name"],
            "tagline": character_config["tagline"],
            "accent": character_config["accent"],
            "bounds_min": bounds_min,
            "bounds_max": bounds_max,
            "groups": groups_out,
            "type_restrictions": template["type_restrictions"],
            "trait_restrictions": template["trait_restrictions"],
        })
    return catalog

def write_string(out: Any, value: str) -> None:
    encoded = value.encode("utf-8")
    out.write(struct.pack("<I", len(encoded)))
    out.write(encoded)


def write_catalog(path: Path, catalog: list[dict[str, Any]]) -> None:
    with path.open("wb") as out:
        out.write(struct.pack("<4sII", CATALOG_MAGIC, CATALOG_VERSION, len(catalog)))
        for character in catalog:
            for value in (character["id"], character["name"], character["tagline"]):
                write_string(out, value)
            out.write(struct.pack("<9f", *character["accent"], *character["bounds_min"], *character["bounds_max"]))
            out.write(struct.pack("<I", len(character["groups"])))
            for group in character["groups"]:
                write_string(out, group["id"])
                write_string(out, group["name"])
                out.write(struct.pack("<B3xiI", 1 if group["required"] else 0, int(group["default"]), len(group["options"])))
                for option in group["options"]:
                    for value in (option["id"], option["name"], option["asset"], option["thumbnail"]):
                        write_string(out, value)
                    out.write(struct.pack("<I", len(option["types"])))
                    for value in option["types"]:
                        write_string(out, value)
                    out.write(struct.pack("<B3xiI", option["variant_kind"], option.get("default_variant", 0 if option["variants"] else -1), len(option["variants"])))
                    for variant in option["variants"]:
                        write_string(out, variant["id"])
                        write_string(out, variant["name"])
                        out.write(struct.pack("<I", len(variant["paths"])))
                        for value in variant["paths"]:
                            write_string(out, value)
                        out.write(struct.pack("<I", len(variant["colors"])))
                        for color in variant["colors"]:
                            out.write(struct.pack("<3f", *color))
            type_restrictions = character["type_restrictions"]
            out.write(struct.pack("<I", len(type_restrictions)))
            for source_type, restricted in type_restrictions.items():
                write_string(out, str(source_type))
                restricted_values = restricted if isinstance(restricted, list) else [restricted]
                out.write(struct.pack("<I", len(restricted_values)))
                for value in restricted_values:
                    write_string(out, str(value))
            trait_restrictions = character["trait_restrictions"]
            out.write(struct.pack("<I", len(trait_restrictions)))
            for group_id, restriction in trait_restrictions.items():
                write_string(out, str(group_id))
                restricted_traits = restriction.get("restrictedTraits", [])
                restricted_types = restriction.get("restrictedTypes", [])
                out.write(struct.pack("<I", len(restricted_traits)))
                for value in restricted_traits:
                    write_string(out, str(value))
                out.write(struct.pack("<I", len(restricted_types)))
                for value in restricted_types:
                    write_string(out, str(value))


def convert(config_path: Path, asset_root: Path, animation_root: Path,
            pose_animation_root: Path, assimp: Path, output_dir: Path) -> None:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="m3d-animations-") as temporary:
        animation_samples = build_animation_samples(assimp, animation_root, pose_animation_root, Path(temporary))
    # Keep the last successful runtime assets intact until all source animations
    # have been validated and converted.
    traits_dir = output_dir / "traits"
    if traits_dir.exists():
        shutil.rmtree(traits_dir)
    catalog = build_catalog(config, asset_root, output_dir, animation_samples)
    write_catalog(output_dir / "catalog.m3c", catalog)
    (output_dir / "catalog.json").write_text(json.dumps({"characters": catalog}, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output_dir / 'catalog.m3c'} with {len(catalog)} characters")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--asset-root", type=Path, required=True)
    parser.add_argument("--animation-root", type=Path, required=True)
    parser.add_argument("--pose-animation-root", type=Path, required=True)
    parser.add_argument("--assimp", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    convert(args.config.resolve(), args.asset_root.resolve(), args.animation_root.resolve(),
            args.pose_animation_root.resolve(), args.assimp.resolve(), args.output_dir.resolve())


if __name__ == "__main__":
    main()
