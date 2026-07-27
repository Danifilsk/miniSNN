from __future__ import annotations

import csv
import html
import os
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "results" / "d1_b_robustness"


def read_text(name: str) -> str:
    path = OUTPUT / name
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else "NA"


def csv_table(name: str) -> str:
    path = OUTPUT / name
    if not path.is_file():
        return "<p>NA</p>"
    with path.open(newline="", encoding="utf-8", errors="replace") as file:
        rows = list(csv.reader(file))
    if not rows:
        return "<p>CSV vazio.</p>"
    header = "".join(f"<th>{html.escape(value)}</th>" for value in rows[0])
    body = "".join(
        "<tr>" + "".join(f"<td>{html.escape(value)}</td>" for value in row) + "</tr>"
        for row in rows[1:]
    )
    return f"<table><thead><tr>{header}</tr></thead><tbody>{body}</tbody></table>"


def main() -> int:
    if not OUTPUT.is_dir():
        print("D1-B report FAILED: output directory absent")
        return 1
    report = OUTPUT / "d1_b_report.html"
    temporary = report.with_suffix(".html.tmp")
    links = ("config_source.ini", "config_used.ini", "d1_b_determinism.csv",
             "d1_b_optimization_determinism.csv", "d1_b_corruption.csv", "d1_b_stress.csv", "d1_b_long_runs.csv",
             "d1_b_performance.csv", "d1_b_sanitizers.txt", "d1_b_symbols.txt",
             "d1_b_posix_headless.txt", "d1_b_summary.txt", "d1_b_manifest.txt")
    link_html = "".join(
        f'<li><a href="{html.escape(name)}">{html.escape(name)}</a></li>'
        for name in links if (OUTPUT / name).is_file()
    )
    document = f"""<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><title>D1-B robustness audit</title>
<style>body{{background:#12161c;color:#e6edf3;font:14px system-ui,sans-serif;margin:2rem}}h1,h2{{color:#7dd3fc}}table{{border-collapse:collapse;width:100%;margin:.75rem 0}}th,td{{border:1px solid #334155;padding:.35rem;text-align:left}}th{{background:#1e293b}}pre{{white-space:pre-wrap;background:#0b1016;padding:1rem}}a{{color:#7dd3fc}}</style>
</head><body><h1>D1-B robustness, determinism, stress and performance</h1>
<p>Relatorio local. Resultados sao especificos desta execucao; tempos dependem da maquina.</p>
<h2>Configuracao efetiva</h2><pre>{html.escape(read_text('config_used.ini'))}</pre>
<h2>Determinismo</h2>{csv_table('d1_b_determinism.csv')}
<h2>O0 versus O2</h2>{csv_table('d1_b_optimization_determinism.csv')}
<h2>Corrupcao e parsing</h2>{csv_table('d1_b_corruption.csv')}
<h2>Stress e ciclo de vida</h2>{csv_table('d1_b_stress.csv')}
<h2>Long runs e checkpoint</h2>{csv_table('d1_b_long_runs.csv')}
<h2>Desempenho</h2>{csv_table('d1_b_performance.csv')}
<h2>Sanitizers</h2><pre>{html.escape(read_text('d1_b_sanitizers.txt'))}</pre>
<h2>Smoke POSIX headless</h2><pre>{html.escape(read_text('d1_b_posix_headless.txt'))}</pre>
<h2>Simbolos e ABI</h2><pre>{html.escape(read_text('d1_b_symbols.txt'))}</pre>
<h2>Limitacoes e pendencias D1-C</h2><p>ASan/UBSan so contam como executados quando o artefato declara PASS. ABI binaria entre toolchains nao e garantida. D1 ainda nao esta concluido e Core v1.0-rc nao foi declarado.</p>
<h2>Artefatos</h2><ul>{link_html}</ul></body></html>
"""
    temporary.write_text(document, encoding="utf-8")
    os.replace(temporary, report)
    print("D1-B local HTML report generated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
