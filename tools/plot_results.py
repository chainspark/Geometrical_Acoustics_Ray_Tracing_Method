#!/usr/bin/env python3
"""Plot CSV output of the `gart` CLI.

  python tools/plot_results.py case2.csv           # SPL vs distance: ray traced vs image source
  python tools/plot_results.py grid.csv            # 3D surface of the SPL map (like docs/figures)
  python tools/plot_results.py grid.csv --2d       # heat map instead of a surface
  python tools/plot_results.py grid.csv --compare  # ray traced and exact surfaces side by side

Requires numpy and matplotlib.
"""
import argparse

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm


def plot_sweep(data, title):
    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.plot(data["d"], data["spl_analytical"], label="image source (exact)", lw=2)
    ax.plot(data["d"], data["spl_raytraced"], "o-", ms=3, lw=1, label="ray traced")
    ax.set_xlabel("receiver displacement d (m)")
    ax.set_ylabel("SPL (dB re 20 µPa)")
    ax.set_title(title)
    ax.grid(alpha=0.3)
    ax.legend()
    err = data["error_db"][np.isfinite(data["error_db"])]
    print(f"mean |error| {np.mean(np.abs(err)):.2f} dB, RMS {np.sqrt(np.mean(err ** 2)):.2f} dB")
    return fig


def grid_values(data, column):
    xs, ys = np.unique(data["x"]), np.unique(data["y"])
    v = data[column].reshape(len(ys), len(xs))
    return xs, ys, np.where(np.isfinite(v), v, np.nan)


def surface(fig, ax, xs, ys, spl, title, vmin, vmax):
    X, Y = np.meshgrid(xs, ys)
    surf = ax.plot_surface(X, Y, spl, cmap=cm.magma, vmin=vmin, vmax=vmax,
                           rstride=1, cstride=1, linewidth=0, antialiased=False)
    ax.set_zlim(vmin, vmax)
    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.set_zlabel("SPL (dB)")
    ax.set_title(title)
    ax.view_init(elev=28, azim=-60)
    return surf


def plot_grid(data, title, flat, compare):
    xs, ys, spl = grid_values(data, "spl_raytraced")
    _, _, exact = grid_values(data, "spl_analytical")
    # Common colour/z range: from 35 dB below the peak (clips the source singularity's tail).
    vmax = np.nanpercentile(exact, 99.5)
    vmin = max(np.nanmin(exact), vmax - 35)
    spl_c, exact_c = np.clip(spl, vmin, vmax), np.clip(exact, vmin, vmax)

    if flat:
        fig, ax = plt.subplots(figsize=(7, 6))
        im = ax.pcolormesh(xs, ys, spl, cmap=cm.magma, shading="nearest", vmin=vmin, vmax=vmax)
        ax.set_aspect("equal")
        ax.set_xlabel("x (m)")
        ax.set_ylabel("y (m)")
        ax.set_title(title)
        fig.colorbar(im, ax=ax, label="SPL (dB)")
        return fig

    if compare:
        fig = plt.figure(figsize=(14, 5.6))
        a1 = fig.add_axes([0.0, 0.02, 0.44, 0.86], projection="3d")
        a2 = fig.add_axes([0.44, 0.02, 0.44, 0.86], projection="3d")
        surf = surface(fig, a1, xs, ys, spl_c, "ray traced", vmin, vmax)
        surface(fig, a2, xs, ys, exact_c, "image source (exact)", vmin, vmax)
        fig.suptitle(title, y=0.97)
        cax = fig.add_axes([0.92, 0.2, 0.012, 0.55])
        fig.colorbar(surf, cax=cax, label="SPL (dB)")
        return fig

    fig = plt.figure(figsize=(8, 6))
    ax = fig.add_subplot(projection="3d")
    surf = surface(fig, ax, xs, ys, spl_c, title, vmin, vmax)
    fig.colorbar(surf, shrink=0.55, aspect=10, label="SPL (dB)")
    return fig


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("csv")
    p.add_argument("--title", default=None)
    p.add_argument("--2d", dest="flat", action="store_true", help="heat map instead of 3D surface")
    p.add_argument("--compare", action="store_true", help="ray traced and exact surfaces side by side")
    p.add_argument("--save", help="write the figure to this file instead of showing it")
    a = p.parse_args()

    data = np.genfromtxt(a.csv, delimiter=",", names=True)
    title = a.title or a.csv
    if "x" in data.dtype.names:
        fig = plot_grid(data, title, a.flat, a.compare)
    else:
        fig = plot_sweep(data, title)
        fig.tight_layout()
    if a.save:
        fig.savefig(a.save, dpi=130, bbox_inches="tight", pad_inches=0.15)
    else:
        plt.show()


if __name__ == "__main__":
    main()
