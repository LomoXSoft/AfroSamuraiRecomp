#!/usr/bin/env python3
"""The four language tables must line up with the Text fields.

A block of strings added at the end of each language, while the fields were
inserted earlier, made the page after the language choice show the
'game is running' sentence as its title.
"""
from __future__ import annotations

import pathlib
import re
import sys

CPP = pathlib.Path(__file__).resolve().parents[1] / "tools" / "afrosetup.cpp"


def decode(body: str) -> str:
    out = []
    i = 0
    while i < len(body):
        if body[i] != "\\":
            out.append(body[i])
            i += 1
            continue
        i += 1
        if body.startswith("u", i):
            hexes = body[i + 1 : i + 5]
            out.append(chr(int(hexes, 16)))
            i += 5
        elif body[i] == "n":
            out.append("\n")
            i += 1
        else:
            out.append(body[i])
            i += 1
    return "".join(out)


def strings_of(blob: str) -> list[str]:
    vals = []
    i = 0
    while True:
        start = blob.find('L"', i)
        if start < 0:
            break
        i = start + 2
        buf = []
        while i < len(blob):
            if blob[i] == "\\" and i + 1 < len(blob):
                if blob[i + 1] == "u":
                    buf.append(blob[i : i + 6])
                    i += 6
                    continue
                buf.append(blob[i : i + 2])
                i += 2
                continue
            if blob[i] == '"':
                vals.append(decode("".join(buf)))
                i += 1
                break
            buf.append(blob[i])
            i += 1
    return vals


def main() -> None:
    text = CPP.read_text(encoding="utf-8")
    struct = text.split("struct Text {", 1)[1].split("};", 1)[0]
    fields = re.findall(r"\b([a-z][a-z0-9_]*)\b", struct)
    # drop the type name leftovers; the pattern already skips wchar_t/const.
    fields = [name for name in fields if name not in {"const", "wchar_t"}]
    init = text.split("const Text kText[] = {", 1)[1].split("};", 1)[0]
    langs = []
    depth = 0
    start = None
    for i, ch in enumerate(init):
        if ch == "{":
            if depth == 0:
                start = i + 1
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0 and start is not None:
                langs.append(strings_of(init[start:i]))
                start = None
    if len(langs) != 4:
        raise SystemExit(f"expected 4 languages, found {len(langs)}")
    names = ("en", "es", "fr", "it")
    for name, vals in zip(names, langs):
        if len(vals) != len(fields):
            raise SystemExit(f"{name}: {len(vals)} strings, {len(fields)} fields")
    table = {lang: dict(zip(fields, vals)) for lang, vals in zip(names, langs)}
    expect = {
        "es": {
            "mode_title": "Instalar o actualizar",
            "mode_install": "Instalar",
            "mode_update": "Actualizar",
            "upd_running": "Afrorecomp está en ejecución. Ciérrelo. Este programa no lo detendrá.",
            "run_stop_yes": "Detener",
            "run_stop_no": "Continuar",
            "done_title": "Instalación finalizada",
            "need_license": "Acepte la licencia para continuar.",
            "space_approx": "Espacio aproximado que liberarían las herramientas: 7,9 GB",
        },
        "en": {
            "mode_title": "Install or update",
            "mode_install": "Install",
            "mode_update": "Update",
            "upd_running": "Afrorecomp is running. Close it. This setup will not stop it.",
            "run_stop_yes": "Stop",
            "done_title": "Installation finished",
            "space_approx": "Approximate space the tools would free: 7.9 GB",
        },
        "fr": {
            "mode_title": "Installer ou mettre à jour",
            "mode_install": "Installer",
            "mode_update": "Mettre à jour",
            "run_stop_yes": "Arrêter",
            "space_approx": "Espace approximatif que les outils libéreraient : 7,9 Go",
        },
        "it": {
            "mode_title": "Installa o aggiorna",
            "mode_install": "Installa",
            "mode_update": "Aggiorna",
            "run_stop_yes": "Ferma",
            "space_approx": "Spazio approssimativo che gli strumenti libererebbero: 7,9 GB",
        },
    }
    for lang, wanted in expect.items():
        for field, value in wanted.items():
            got = table[lang][field]
            if got != value:
                raise SystemExit(f"{lang}.{field}:\n  got {got!r}\n  want {value!r}")
        if table[lang]["mode_title"] == table[lang]["upd_running"]:
            raise SystemExit(f"{lang}: mode title collided with the running-game warning")
    print(f"ui text alignment: {len(fields)} fields x 4 languages")


if __name__ == "__main__":
    try:
        main()
    except SystemExit as exc:
        print(exc, file=sys.stderr)
        raise
