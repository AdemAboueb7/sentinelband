#!/usr/bin/env python3
"""
analyse_s11.py — SentinelBand
Analyse des données S11 VNA / CST et génération de la figure de comparaison.

Usage :
    python analyse_s11.py

Formats supportés :
    - Touchstone .s1p  (RI, MA ou DB)
    - CSV deux colonnes  freq_Hz,S11_dB  ou  freq_GHz,S11_dB

Résultats affichés :
    fr (GHz), S11min (dB), fL (GHz), fH (GHz), BW (MHz),
    Δfr (GHz), Δfr (%)

Livrable : s11_comparaison.pdf
"""

import os
import sys
import re
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")          # sans interface graphique
import matplotlib.pyplot as plt

# ── Référence à −10 dB ────────────────────────────────────────────────────────
REF_DB = -10.0

# ── Correspondance fichiers → étiquettes ──────────────────────────────────────
# Adapter les noms de fichiers à ceux réellement présents dans le dossier.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.join(BASE_DIR, "..")   # dossier racine du projet

DATASETS = [
    # (chemin relatif au dossier projet, étiquette, style matplotlib)
    ("figures/s11_simulation.s1p",  "Simulation CST",          "b-",  2.0),
    ("figures/s11_plane.s1p",       "Mesure plane",            "r--", 1.8),
    ("figures/s11_curved.s1p",      "Mesure courbée",          "g-.", 1.8),
    ("figures/s11_onbody.s1p",      "Mesure proche du corps",  "m:",  1.8),
    # Variantes CSV (si les .s1p ne sont pas disponibles)
    ("figures/s11_simulation.csv",  "Simulation CST",          "b-",  2.0),
    ("figures/s11_plane.csv",       "Mesure plane",            "r--", 1.8),
    ("figures/s11_curved.csv",      "Mesure courbée",          "g-.", 1.8),
    ("figures/s11_onbody.csv",      "Mesure proche du corps",  "m:",  1.8),
]


def parse_s1p(path):
    """
    Lit un fichier Touchstone .s1p.
    Retourne (freq_GHz : ndarray, s11_dB : ndarray).
    Formats gérés : RI, MA, DB (insensible à la casse).
    """
    freqs, s11_db = [], []
    freq_mult = 1.0   # Hz par défaut
    fmt = "DB"
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            # En-têtes et commentaires
            if line.startswith("!"):
                continue
            if line.startswith("#"):
                tokens = line.upper().split()
                # Unité de fréquence
                for u, m in [("HZ", 1e0), ("KHZ", 1e3), ("MHZ", 1e6), ("GHZ", 1e9)]:
                    if u in tokens:
                        freq_mult = m
                # Format
                for f in ["DB", "MA", "RI"]:
                    if f in tokens:
                        fmt = f
                continue
            tokens = line.split()
            if len(tokens) < 3:
                continue
            try:
                f = float(tokens[0]) * freq_mult / 1e9   # → GHz
                v1, v2 = float(tokens[1]), float(tokens[2])
                if fmt == "DB":
                    db = v1
                elif fmt == "MA":
                    db = 20 * math.log10(v1) if v1 > 0 else -300
                elif fmt == "RI":
                    mag = math.sqrt(v1**2 + v2**2)
                    db = 20 * math.log10(mag) if mag > 0 else -300
                else:
                    db = v1
                freqs.append(f)
                s11_db.append(db)
            except ValueError:
                pass
    return np.array(freqs), np.array(s11_db)


def parse_csv(path):
    """
    Lit un CSV  freq,S11_dB.
    Détecte automatiquement si freq est en Hz ou GHz.
    """
    freqs, s11_db = [], []
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or re.match(r"[a-zA-Z]", line):
                continue
            tokens = re.split(r"[,;\t ]+", line)
            if len(tokens) < 2:
                continue
            try:
                f = float(tokens[0])
                db = float(tokens[1])
                freqs.append(f)
                s11_db.append(db)
            except ValueError:
                pass
    freqs = np.array(freqs)
    # Détection Hz vs GHz : si la moyenne est > 1e6 → Hz
    if freqs.mean() > 1e6:
        freqs /= 1e9
    return freqs, np.array(s11_db)


def load(path):
    """Charge un fichier .s1p ou .csv."""
    ext = os.path.splitext(path)[1].lower()
    if ext == ".s1p":
        return parse_s1p(path)
    elif ext == ".csv":
        return parse_csv(path)
    else:
        raise ValueError(f"Format non supporté : {ext}")


def analyze(freqs, s11_db):
    """
    Calcule fr, S11min, fL, fH, BW à −10 dB.
    Retourne un dict.
    """
    idx_min = np.argmin(s11_db)
    fr = freqs[idx_min]
    s11_min = s11_db[idx_min]

    # Bande passante à REF_DB
    below = s11_db <= REF_DB
    fl, fh = None, None

    # Limite basse : chercher le premier passage sous REF_DB avant fr
    idxs_below = np.where(below)[0]
    if len(idxs_below) == 0:
        bw = 0.0
    else:
        # fL : premier indice où on passe sous −10 dB dans la région à gauche de fr
        left = idxs_below[idxs_below <= idx_min]
        right = idxs_below[idxs_below >= idx_min]
        fl = freqs[left[0]]  if len(left)  else freqs[idx_min]
        fh = freqs[right[-1]] if len(right) else freqs[idx_min]
        bw = (fh - fl) * 1000   # MHz

    return {
        "fr_GHz":   fr,
        "s11_min":  s11_min,
        "fl_GHz":   fl,
        "fh_GHz":   fh,
        "bw_MHz":   bw,
    }


def print_table(label, res):
    print(f"\n{'─'*55}")
    print(f"  {label}")
    print(f"{'─'*55}")
    print(f"  Fréquence de résonance fr : {res['fr_GHz']:.4f} GHz")
    print(f"  S11 minimal               : {res['s11_min']:.2f} dB")
    if res["fl_GHz"]:
        print(f"  Limite basse fL           : {res['fl_GHz']:.4f} GHz")
        print(f"  Limite haute fH           : {res['fh_GHz']:.4f} GHz")
        print(f"  Bande passante à −10 dB   : {res['bw_MHz']:.1f} MHz")
    else:
        print("  Bande passante            : < −10 dB non atteint")


def delta(res_sim, res_mes, label_mes):
    dfr = res_mes["fr_GHz"] - res_sim["fr_GHz"]
    dfr_pct = dfr / res_sim["fr_GHz"] * 100
    print(f"\n  Écart simulation / {label_mes}")
    print(f"    Δfr         = {dfr:+.4f} GHz")
    print(f"    Δfr (%)     = {dfr_pct:+.2f} %")
    return dfr, dfr_pct


def main():
    loaded = []
    for rel, label, style, lw in DATASETS:
        path = os.path.join(PROJECT_DIR, rel)
        if os.path.isfile(path):
            try:
                freqs, s11 = load(path)
                if len(freqs) < 2:
                    print(f"[AVERTISSEMENT] {rel} : trop peu de points.")
                    continue
                loaded.append((freqs, s11, label, style, lw))
                print(f"[OK] Chargé : {rel}  ({len(freqs)} points, "
                      f"f = {freqs[0]:.3f}–{freqs[-1]:.3f} GHz)")
            except Exception as e:
                print(f"[ERREUR] {rel} : {e}")

    if not loaded:
        print("\n[ERREUR] Aucun fichier de données S11 trouvé.")
        print("Placez les fichiers dans le dossier 'figures/' :")
        print("  s11_simulation.s1p (ou .csv)")
        print("  s11_plane.s1p      (ou .csv)")
        print("  s11_curved.s1p     (ou .csv)")
        print("  s11_onbody.s1p     (ou .csv)  [optionnel]")
        sys.exit(1)

    # ── Analyse par courbe ────────────────────────────────────────────────────
    results = {}
    for freqs, s11, label, *_ in loaded:
        r = analyze(freqs, s11)
        results[label] = r
        print_table(label, r)

    # ── Écarts simulation–mesure ──────────────────────────────────────────────
    sim_label = next((l for _, _, l, *_ in loaded if "CST" in l or "imulation" in l), None)
    if sim_label:
        print(f"\n{'═'*55}")
        print("  Écarts simulation–mesure")
        print(f"{'═'*55}")
        for label, r in results.items():
            if label == sim_label:
                continue
            dfr, dfr_pct = delta(results[sim_label], r, label)
            print(f"    Cohérence : fr ∈ [2.4–2.5] GHz ? "
                  f"{'OUI ✓' if 2.3 <= r['fr_GHz'] <= 2.6 else 'NON — vérifier'}")

    # ── Figure de comparaison ─────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(9, 5))

    for freqs, s11, label, style, lw in loaded:
        ax.plot(freqs, s11, style, linewidth=lw, label=label)

    ax.axhline(REF_DB, color="k", linestyle="--", linewidth=1.2,
               label=f"Référence {REF_DB} dB")
    ax.set_xlabel("Fréquence (GHz)", fontsize=12)
    ax.set_ylabel("$S_{11}$ (dB)", fontsize=12)
    ax.set_title("Coefficient de réflexion $S_{11}$ — Simulation CST vs. Mesures VNA\n"
                 "Antenne patch textile 2.45 GHz — SentinelBand", fontsize=11)
    ax.legend(fontsize=10)
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.set_ylim(bottom=min(-40, ax.get_ylim()[0]))

    # Marquer les fréquences de résonance
    colors_marker = {"Simulation CST": "blue", "Mesure plane": "red",
                     "Mesure courbée": "green", "Mesure proche du corps": "magenta"}
    for label, r in results.items():
        c = colors_marker.get(label, "gray")
        ax.axvline(r["fr_GHz"], color=c, linestyle=":", linewidth=0.8, alpha=0.5)

    fig.tight_layout()
    out = os.path.join(PROJECT_DIR, "figures", "s11_comparaison.pdf")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"\n[OK] Figure sauvegardée : {out}")
    print("\nScript terminé.")


if __name__ == "__main__":
    main()
