from __future__ import annotations

from decimal import Decimal, InvalidOperation
from pathlib import Path
import csv
import heapq
import html
import re
from urllib.parse import quote


METRICS_REPORT_FILENAME = "metrics_report.html"
WEIGHTS_REPORT_FILENAME = "weights_report.html"
REWARD_REPORT_FILENAME = "reward_report.html"
HISTORY_REPORT_FILENAME = "history.html"
EVOLUTION_REPORT_FILENAME = "evolution_report.html"
EVOLUTION_HISTORY_FILENAME = "history.html"
WEIGHT_TABLE_LIMIT = 500
RANKING_LIMIT = 10


class ReportGenerationError(ValueError):
    pass


def escaped(value: object) -> str:
    return html.escape(str(value), quote=True)


def _validate_headers(path: Path, fieldnames: list[str] | None) -> list[str]:
    if not fieldnames or any(name is None or not name.strip() for name in fieldnames):
        raise ReportGenerationError(f"{path.name}: cabecalho ausente ou invalido")
    if len(set(fieldnames)) != len(fieldnames):
        raise ReportGenerationError(f"{path.name}: cabecalho possui colunas duplicadas")
    return fieldnames


def _validate_row(path: Path, row: dict[str | None, str | list[str] | None], line: int) -> dict[str, str]:
    if None in row or any(value is None for value in row.values()):
        raise ReportGenerationError(f"{path.name}: linha {line} incompleta")
    result = {str(key): str(value).strip() for key, value in row.items()}
    for key, value in result.items():
        if value.lower() in {"nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity", "+infinity", "-infinity"}:
            raise ReportGenerationError(
                f"{path.name}: valor nao finito na linha {line}, coluna {key}"
            )
    return result


def read_single_row_csv(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise ReportGenerationError(f"arquivo obrigatorio ausente: {path.name}")
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            _validate_headers(path, reader.fieldnames)
            first = next(reader, None)
            if first is None:
                raise ReportGenerationError(f"{path.name}: CSV vazio")
            row = _validate_row(path, first, 2)
            if next(reader, None) is not None:
                raise ReportGenerationError(
                    f"{path.name}: esperada exatamente uma linha de dados"
                )
            return row
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(f"nao foi possivel ler {path.name}: {error}") from error


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.is_file():
        return values
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return values
    for line in lines:
        if "=" in line and not line.lstrip().startswith(("#", ";")):
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def _decimal(value: object) -> Decimal | None:
    text = str(value).strip()
    if not text or text.upper() == "NA":
        return None
    try:
        number = Decimal(text)
    except InvalidOperation:
        return None
    if not number.is_finite():
        raise ReportGenerationError(f"valor numerico nao finito: {text}")
    return number


def _flag(value: object) -> bool | None:
    text = str(value).strip().lower()
    if text in {"true", "yes", "sim", "1", "on"}:
        return True
    if text in {"false", "no", "nao", "0", "off"}:
        return False
    return None


def _plain_decimal(number: Decimal, significant: int = 9) -> str:
    absolute = abs(number)
    if number != 0 and (absolute >= Decimal("1e12") or absolute < Decimal("1e-6")):
        return format(number, f".{significant}E")
    text = format(number, "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


def format_value(key: str, value: object) -> str:
    text = str(value).strip()
    if not text or text.upper() == "NA":
        return "NA"
    flag = _flag(text)
    if key.endswith("enabled") and flag is not None:
        return "ON" if flag else "OFF"
    number = _decimal(text)
    if number is None:
        return text
    if "weight" in key or "peso" in key:
        if number != 0 and (abs(number) >= Decimal("1e12") or abs(number) < Decimal("1e-6")):
            return _plain_decimal(number, 6)
        quantized = format(number, ".6f").rstrip("0").rstrip(".")
        return quantized or "0"
    if key.endswith("_seconds") or "time_seconds" in key:
        return format(number, ".9g")
    rendered = _plain_decimal(number)
    if "fraction" in key and Decimal("0") <= number <= Decimal("1"):
        return f"{rendered} ({float(number) * 100.0:.2f}%)"
    return rendered


def _atomic_write(path: Path, content: str) -> None:
    temporary = path.with_name(path.name + ".tmp")
    try:
        temporary.write_text(content, encoding="utf-8", newline="\n")
        temporary.replace(path)
    except OSError as error:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise ReportGenerationError(f"nao foi possivel gravar {path.name}: {error}") from error


def _document(title: str, subtitle: str, body: str) -> str:
    return f"""<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{escaped(title)}</title>
<style>
:root {{ color-scheme: dark; --bg:#0b0d10; --panel:#15191f; --line:#343b45; --text:#f4f6f8; --muted:#aeb7c2; --cyan:#61dafb; --green:#75e6a4; --red:#ff8b8b; --amber:#ffd166; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; background:var(--bg); color:var(--text); font-family:"Fixedsys","Courier New",monospace; line-height:1.45; }}
main {{ width:min(1500px,100%); margin:auto; padding:24px; }}
h1 {{ margin:0; font-size:clamp(1.45rem,3vw,2.25rem); text-transform:uppercase; }}
h2 {{ margin:36px 0 12px; padding-bottom:8px; border-bottom:1px solid var(--line); font-size:1.1rem; text-transform:uppercase; }}
h3 {{ margin:22px 0 8px; font-size:1rem; }}
.subtitle,.muted {{ color:var(--muted); }}
.cards {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(190px,1fr)); gap:10px; margin:20px 0; }}
.card {{ background:var(--panel); border:1px solid var(--line); border-radius:6px; padding:14px; min-width:0; }}
.card .label {{ color:var(--muted); font-size:.78rem; text-transform:uppercase; }}
.card .value {{ margin-top:6px; font-size:1.18rem; overflow-wrap:anywhere; }}
.notice {{ border-left:4px solid var(--amber); background:#211d12; padding:12px 14px; margin:14px 0; }}
.ok {{ border-left-color:var(--green); background:#102018; }}
.table-wrap {{ overflow:auto; max-height:620px; border:1px solid var(--line); }}
table {{ width:100%; border-collapse:collapse; font-size:.88rem; }}
th,td {{ padding:8px 10px; border-bottom:1px solid var(--line); text-align:left; white-space:nowrap; }}
th {{ position:sticky; top:0; z-index:1; background:#242a32; }}
tbody tr:nth-child(even) {{ background:#11151a; }}
td.value {{ white-space:normal; overflow-wrap:anywhere; }}
.increase {{ border-left:3px solid var(--green); }} .decrease {{ border-left:3px solid var(--red); }}
.neutral {{ border-left:3px solid var(--line); }} .secondary {{ opacity:.72; }}
.up {{ color:var(--green); }} .down {{ color:var(--red); }}
.history-controls {{ display:flex; flex-wrap:wrap; gap:10px; margin:18px 0; align-items:end; }}
.history-controls label {{ display:grid; gap:5px; color:var(--muted); font-size:.78rem; text-transform:uppercase; }}
.history-controls input,.history-controls select {{ min-width:180px; padding:9px 10px; color:var(--text); background:#0d1116; border:1px solid var(--line); font:inherit; }}
.status-ok,.status-error {{ display:inline-block; border:1px solid currentColor; padding:2px 6px; font-weight:bold; }}
.status-ok {{ color:var(--green); }} .status-error {{ color:var(--red); }}
.artifact-links {{ display:flex; flex-wrap:wrap; gap:6px; max-width:520px; white-space:normal; }}
.artifact-links a {{ border:1px solid var(--line); padding:3px 6px; text-decoration:none; }}
.history-empty {{ padding:18px; border:1px dashed var(--line); color:var(--muted); }}
a {{ color:var(--cyan); }} a:focus,a:hover {{ outline:1px solid var(--cyan); }}
.files {{ display:flex; flex-wrap:wrap; gap:8px; padding:0; list-style:none; }}
.files a {{ display:block; border:1px solid var(--line); padding:7px 9px; text-decoration:none; }}
img {{ display:block; max-width:100%; height:auto; margin:12px 0; border:1px solid var(--line); }}
pre {{ max-height:360px; overflow:auto; padding:12px; background:#080a0c; border:1px solid var(--line); white-space:pre-wrap; overflow-wrap:anywhere; }}
code {{ color:#d8e8f2; }}
@media (max-width:640px) {{ main {{ padding:14px; }} th,td {{ padding:7px; }} }}
</style>
</head>
<body><main>
<header><h1>{escaped(title)}</h1><p class="subtitle">{escaped(subtitle)}</p></header>
{body}
</main></body></html>
"""


def _cards(items: list[tuple[str, object]]) -> str:
    cards = []
    for label, value in items:
        if value is None or str(value).strip() in {"", "NA"}:
            continue
        cards.append(
            f'<div class="card"><div class="label">{escaped(label)}</div>'
            f'<div class="value">{escaped(value)}</div></div>'
        )
    return '<div class="cards">' + "".join(cards) + "</div>" if cards else ""


def _key_value_table(rows: list[tuple[str, object]]) -> str:
    if not rows:
        return '<p class="muted">Nenhum campo disponivel.</p>'
    content = []
    for key, value in rows:
        content.append(
            f"<tr><td><code>{escaped(key)}</code></td>"
            f'<td class="value">{escaped(format_value(key, value))}</td></tr>'
        )
    return '<div class="table-wrap"><table><thead><tr><th>Campo</th><th>Valor</th></tr></thead><tbody>' + "".join(content) + "</tbody></table></div>"


def _existing_file_links(run_dir: Path, filenames: tuple[str, ...]) -> str:
    links = []
    for filename in filenames:
        if (run_dir / filename).is_file():
            href = escaped(quote(filename))
            links.append(f'<li><a href="{href}">{escaped(filename)}</a></li>')
    if not links:
        return '<p class="muted">Nenhum arquivo adicional disponivel.</p>'
    return '<ul class="files">' + "".join(links) + "</ul>"


def _preview_images(run_dir: Path, filenames: tuple[str, ...]) -> str:
    images = []
    for filename in filenames:
        if (run_dir / filename).is_file():
            source = escaped(quote(filename))
            images.append(
                f'<h3>{escaped(filename)}</h3><a href="{source}">'
                f'<img src="{source}" alt="{escaped(filename)}"></a>'
            )
    return "".join(images)


def _first(values: dict[str, str], *keys: str) -> str | None:
    for key in keys:
        value = values.get(key)
        if value is not None and value.strip() not in {"", "NA"}:
            return value
    return None


def _metric_category(key: str) -> str:
    if key in {"run_name", "actual_run_name", "run_path", "timestamp", "topology", "diagnostics_level"} or key.startswith("run_"):
        return "1. Identificacao da execucao"
    if key.startswith("plasticity_"):
        return "12. Plasticidade"
    if key.startswith("homeostasis_"):
        return "13. Homeostase"
    if key.startswith("reward_"):
        return "14. Recompensa, punicao e elegibilidade"
    if key.startswith("performance_"):
        return "14. Desempenho"
    if key.startswith(("exc_", "inh_", "exc_inh_")):
        return "6. EXC/INH"
    if key.startswith("burst_"):
        return "7. Bursts"
    if key.startswith(("voltage_", "current_", "neuron_detailed_")):
        return "10. Neuronio detalhado"
    if key.startswith("network_") and any(token in key for token in ("connection", "weight", "delay", "degree", "density")):
        return "11. Conectividade"
    if key.startswith("network_"):
        return "2. Rede"
    if any(token in key for token in ("gini", "entropy", "top_", "dominant", "concentration", "distribution")):
        return "8. Distribuicao e concentracao"
    if any(token in key for token in ("variance", "std", "_cv", "fano", "synchrony", "correlation")):
        return "9. Variabilidade e sincronia aproximada"
    if key.startswith("neuron_") or key.startswith("isi_"):
        return "5. Neuronios"
    if key.startswith("activity_") and any(token in key for token in ("first", "last", "peak", "streak", "early", "middle", "late", "span")):
        return "4. Atividade temporal"
    if key.startswith(("activity_", "silence_")):
        return "3. Atividade populacional"
    if key.startswith(("diagnostic_", "stability_")):
        return "9. Variabilidade e sincronia aproximada"
    return "15. Metricas adicionais"


def _manifest_metadata(manifest: dict[str, str], metrics: dict[str, str]) -> list[tuple[str, object]]:
    fields = [
        ("run_name", _first(metrics, "run_name") or manifest.get("requested_run_name")),
        ("actual_run_name", _first(metrics, "actual_run_name") or manifest.get("actual_run_name")),
        ("timestamp", _first(metrics, "timestamp") or manifest.get("timestamp")),
        ("topology", _first(metrics, "topology") or manifest.get("topology")),
        ("seed", _first(metrics, "run_seed", "seed") or manifest.get("seed")),
        ("diagnostics_level", _first(metrics, "diagnostics_level") or manifest.get("diagnostics_level")),
        ("miniSNN_version", manifest.get("miniSNN_version")),
        ("git_commit", manifest.get("git_commit")),
    ]
    return [(key, value) for key, value in fields if value not in {None, "", "NA"}]


def _update_manifest(run_dir: Path) -> None:
    path = run_dir / "run_manifest.txt"
    if not path.is_file():
        return
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        kept: list[str] = []
        skipping = False
        for line in lines:
            if line.strip() == "[html_reports]":
                skipping = True
                continue
            if skipping and re.fullmatch(r"\[[^]]+\]", line.strip()):
                skipping = False
            if not skipping:
                kept.append(line)
        while kept and not kept[-1].strip():
            kept.pop()
        reports = [
            filename
            for filename in (
                METRICS_REPORT_FILENAME,
                WEIGHTS_REPORT_FILENAME,
                REWARD_REPORT_FILENAME,
            )
            if (run_dir / filename).is_file()
        ]
        kept.extend(["", "[html_reports]", "report_files=" + ";".join(reports)])
        for filename in reports:
            kept.append(f"{filename.removesuffix('.html')}={filename}")
        _atomic_write(path, "\n".join(kept) + "\n")
    except (OSError, ReportGenerationError):
        return


def generate_metrics_report(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    if not run_dir.is_dir():
        raise ReportGenerationError(f"pasta da execucao inexistente: {run_dir}")
    metrics = read_single_row_csv(run_dir / "metrics.csv")
    plasticity_path = run_dir / "plasticity_metrics.csv"
    if plasticity_path.is_file():
        plasticity = read_single_row_csv(plasticity_path)
        for key, value in plasticity.items():
            if key not in metrics or metrics[key].strip() in {"", "NA"}:
                metrics[key] = value
    homeostasis_path = run_dir / "homeostasis_metrics.csv"
    if homeostasis_path.is_file():
        homeostasis = read_single_row_csv(homeostasis_path)
        for key, value in homeostasis.items():
            if key not in metrics or metrics[key].strip() in {"", "NA"}:
                metrics[key] = value
    reward_path = run_dir / "reward_metrics.csv"
    if reward_path.is_file():
        reward = read_single_row_csv(reward_path)
        for key, value in reward.items():
            if key not in metrics or metrics[key].strip() in {"", "NA"}:
                metrics[key] = value
    manifest = read_key_values(run_dir / "run_manifest.txt")

    cards = [
        ("Spikes totais", format_value("activity_total_spikes", _first(metrics, "activity_total_spikes", "total_spikes", "spikes_total") or "NA")),
        ("Neuronios", format_value("network_num_neurons", _first(metrics, "network_num_neurons", "neurons") or "NA")),
        ("Conexoes", format_value("network_total_connections", _first(metrics, "network_total_connections", "connections") or "NA")),
        ("Fracao de atividade", format_value("activity_fraction", _first(metrics, "activity_fraction") or "NA")),
        ("Primeiro spike", format_value("activity_first_active_step", _first(metrics, "activity_first_active_step", "first_active_step") or "NA")),
        ("Ultimo spike", format_value("activity_last_active_step", _first(metrics, "activity_last_active_step", "last_active_step") or "NA")),
        ("Regime diagnostico", _first(metrics, "diagnostic_regime", "regime") or "NA"),
        ("Stability score", format_value("diagnostic_stability_score", _first(metrics, "diagnostic_stability_score", "stability_score") or "NA")),
    ]
    enabled = _flag(_first(metrics, "plasticity_enabled") or manifest.get("plasticity_enabled", "false"))
    cards.append(("STDP", "ON" if enabled else "OFF"))
    homeostasis_enabled = _flag(
        _first(metrics, "homeostasis_enabled")
        or manifest.get("homeostasis_enabled", "false")
    )
    cards.append(("Homeostase", "ON" if homeostasis_enabled else "OFF"))
    reward_enabled = _flag(
        _first(metrics, "reward_enabled") or manifest.get("reward_enabled", "false")
    )
    cards.append(("Reward", "ON" if reward_enabled else "OFF"))

    sections: dict[str, list[tuple[str, object]]] = {}
    for key, value in metrics.items():
        if key == "run_path":
            continue
        if key.startswith("plasticity_") and enabled is False and key != "plasticity_enabled":
            continue
        if key.startswith("homeostasis_") and homeostasis_enabled is False and key != "homeostasis_enabled":
            continue
        if key.startswith("reward_") and reward_enabled is False and key != "reward_enabled":
            continue
        sections.setdefault(_metric_category(key), []).append((key, value))

    body = _cards(cards)
    body += "<h2>1. Identificacao da execucao</h2>" + _key_value_table(_manifest_metadata(manifest, metrics))
    for title in (
        "2. Rede", "3. Atividade populacional", "4. Atividade temporal",
        "5. Neuronios", "6. EXC/INH", "7. Bursts",
        "8. Distribuicao e concentracao", "9. Variabilidade e sincronia aproximada",
        "10. Neuronio detalhado", "11. Conectividade", "12. Plasticidade",
        "13. Homeostase", "14. Recompensa, punicao e elegibilidade",
        "14. Desempenho", "15. Metricas adicionais",
    ):
        rows = sections.get(title, [])
        if title == "12. Plasticidade" and enabled is False:
            body += '<h2>12. Plasticidade</h2><div class="notice">Plasticidade: DESATIVADA. Zeros de aprendizado nao sao apresentados como medicoes.</div>'
        elif rows:
            body += f"<h2>{escaped(title)}</h2>" + _key_value_table(rows)

    if homeostasis_enabled is False:
        body += '<div class="notice">Homeostase: DESATIVADA. O caminho dinamico permanece o convencional.</div>'
    elif homeostasis_enabled:
        body += '<div class="notice">Mecanismos homeostaticos simplificados; nao garantem estabilidade universal.</div>'

    if any(key in metrics for key in ("diagnostic_regime", "diagnostic_confidence", "diagnostic_stability_score", "activity_synchrony_proxy", "synchrony_proxy", "stability_score")):
        body += '<div class="notice">Indicador heuristico da miniSNN; nao representa uma medida biologica universal.</div>'

    body += "<h2>14. Arquivos e proveniencia</h2>"
    body += _existing_file_links(run_dir, (
        "metrics.csv", "metrics_report.txt", "population.csv", "raster.csv",
        "run_manifest.txt", "diagnostics_overview.png", "plasticity_metrics.csv",
        "weights_initial.csv", "weights_final.csv", "weight_history.csv",
        "stdp_report.txt", "plasticity_overview.png", WEIGHTS_REPORT_FILENAME,
        "homeostasis_metrics.csv", "homeostasis_history.csv",
        "threshold_history.csv", "homeostasis_neurons.csv",
        "homeostasis_report.txt", "homeostasis_report.html",
        "homeostasis_overview.png",
        "reward_metrics.csv", "reward_events.csv", "reward_history.csv",
        "eligibility_history.csv", "reward_connections.csv",
        "reward_report.txt", REWARD_REPORT_FILENAME, "reward_overview.png",
    ))
    body += _preview_images(run_dir, (
        "diagnostics_overview.png", "plasticity_overview.png",
        "homeostasis_overview.png",
        "reward_overview.png",
    ))
    for filename, title in (
        ("metrics_report.txt", "Relatorio textual"),
        ("homeostasis_report.txt", "Relatorio homeostatico textual"),
        ("reward_report.txt", "Relatorio textual de recompensa"),
    ):
        text_report = run_dir / filename
        if text_report.is_file():
            try:
                content = text_report.read_text(
                    encoding="utf-8", errors="replace"
                )[:100_000]
                body += f"<h3>{escaped(title)}</h3><pre>{escaped(content)}</pre>"
            except OSError:
                pass

    output = run_dir / METRICS_REPORT_FILENAME
    actual = _first(metrics, "actual_run_name", "run_name") or run_dir.name
    _atomic_write(output, _document("MINISNN — RELATORIO DE METRICAS", f"Execucao: {actual}", body))
    _update_manifest(run_dir)
    return output


WEIGHT_REQUIRED_COLUMNS = {
    "connection_id", "source", "target", "source_type", "target_type", "delay",
    "eligible", "initial_weight", "final_weight", "signed_change", "absolute_change",
}


def _parse_finite_decimal(path: Path, row: dict[str, str], key: str, line: int) -> Decimal:
    try:
        value = Decimal(row[key])
    except (InvalidOperation, KeyError) as error:
        raise ReportGenerationError(
            f"{path.name}: valor numerico invalido na linha {line}, coluna {key}"
        ) from error
    if not value.is_finite():
        raise ReportGenerationError(
            f"{path.name}: valor nao finito na linha {line}, coluna {key}"
        )
    return value


def _parse_integer_decimal(path: Path, row: dict[str, str], key: str, line: int) -> Decimal:
    value = _parse_finite_decimal(path, row, key, line)
    if value != value.to_integral_value():
        raise ReportGenerationError(
            f"{path.name}: inteiro invalido na linha {line}, coluna {key}"
        )
    return value


def _parse_binary(path: Path, row: dict[str, str], key: str, line: int, default: bool = False) -> bool:
    if key not in row or row[key] == "":
        return default
    flag = _flag(row[key])
    if flag is None:
        raise ReportGenerationError(
            f"{path.name}: valor booleano invalido na linha {line}, coluna {key}"
        )
    return flag


def _push_top(heap: list[tuple[Decimal, int, dict[str, str]]], score: Decimal, serial: int, row: dict[str, str]) -> None:
    item = (score, serial, row.copy())
    if len(heap) < RANKING_LIMIT:
        heapq.heappush(heap, item)
    elif score > heap[0][0]:
        heapq.heapreplace(heap, item)


def _stream_weights(path: Path, weight_min: Decimal | None, weight_max: Decimal | None) -> dict[str, object]:
    displayed: list[dict[str, str]] = []
    increases: list[tuple[Decimal, int, dict[str, str]]] = []
    reductions: list[tuple[Decimal, int, dict[str, str]]] = []
    absolute: list[tuple[Decimal, int, dict[str, str]]] = []
    unchanged: list[dict[str, str]] = []
    count = eligible_count = sampled_count = min_count = max_count = 0
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            headers = set(_validate_headers(path, reader.fieldnames))
            missing = sorted(WEIGHT_REQUIRED_COLUMNS - headers)
            if missing:
                raise ReportGenerationError(
                    f"{path.name}: colunas obrigatorias ausentes: {', '.join(missing)}"
                )
            for line, raw in enumerate(reader, start=2):
                row = _validate_row(path, raw, line)
                for key in ("connection_id", "source", "target", "delay"):
                    _parse_integer_decimal(path, row, key, line)
                for key in ("initial_weight", "final_weight", "signed_change", "absolute_change"):
                    _parse_finite_decimal(path, row, key, line)
                eligible = _parse_binary(path, row, "eligible", line)
                sampled = _parse_binary(path, row, "sampled", line)
                signed = _parse_finite_decimal(path, row, "signed_change", line)
                magnitude = _parse_finite_decimal(path, row, "absolute_change", line)
                if magnitude < 0:
                    raise ReportGenerationError(
                        f"{path.name}: mudanca absoluta negativa na linha {line}"
                    )
                final_weight = _parse_finite_decimal(path, row, "final_weight", line)
                count += 1
                eligible_count += int(eligible)
                sampled_count += int(sampled)
                if len(displayed) < WEIGHT_TABLE_LIMIT:
                    displayed.append(row)
                if signed > 0:
                    _push_top(increases, signed, count, row)
                elif signed < 0:
                    _push_top(reductions, -signed, count, row)
                elif len(unchanged) < RANKING_LIMIT:
                    unchanged.append(row.copy())
                _push_top(absolute, magnitude, count, row)
                if weight_min is not None and final_weight == weight_min:
                    min_count += 1
                if weight_max is not None and final_weight == weight_max:
                    max_count += 1
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(f"nao foi possivel ler {path.name}: {error}") from error
    if count == 0:
        raise ReportGenerationError(f"{path.name}: CSV vazio")
    return {
        "count": count,
        "eligible_count": eligible_count,
        "sampled_count": sampled_count,
        "displayed": displayed,
        "increases": [item[2] for item in sorted(increases, reverse=True)],
        "reductions": [item[2] for item in sorted(reductions, reverse=True)],
        "absolute": [item[2] for item in sorted(absolute, reverse=True)],
        "unchanged": unchanged,
        "min_count": min_count,
        "max_count": max_count,
    }


def _history_summary(path: Path) -> dict[str, object] | None:
    if not path.is_file():
        return None
    first_step: Decimal | None = None
    last_step: Decimal | None = None
    previous_step: Decimal | None = None
    minimum_interval: Decimal | None = None
    connection_ids: set[str] = set()
    rows = 0
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            headers = set(_validate_headers(path, reader.fieldnames))
            required = {"step", "connection_id", "source", "target", "weight"}
            missing = sorted(required - headers)
            if missing:
                raise ReportGenerationError(
                    f"{path.name}: colunas obrigatorias ausentes: {', '.join(missing)}"
                )
            for line, raw in enumerate(reader, start=2):
                row = _validate_row(path, raw, line)
                step = _parse_integer_decimal(path, row, "step", line)
                _parse_integer_decimal(path, row, "connection_id", line)
                _parse_integer_decimal(path, row, "source", line)
                _parse_integer_decimal(path, row, "target", line)
                _parse_finite_decimal(path, row, "weight", line)
                rows += 1
                first_step = step if first_step is None else min(first_step, step)
                last_step = step if last_step is None else max(last_step, step)
                if previous_step is not None and step > previous_step:
                    interval = step - previous_step
                    minimum_interval = interval if minimum_interval is None else min(minimum_interval, interval)
                previous_step = step
                connection_ids.add(row["connection_id"])
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(f"nao foi possivel ler {path.name}: {error}") from error
    return {
        "history_first_step": first_step if first_step is not None else "NA",
        "history_last_step": last_step if last_step is not None else "NA",
        "history_connection_count": len(connection_ids),
        "history_record_interval_observed": minimum_interval if minimum_interval is not None else "NA",
        "history_row_count": rows,
    }


def _weight_change_label(row: dict[str, str]) -> tuple[str, str]:
    signed = Decimal(row["signed_change"])
    eligible = _flag(row.get("eligible", "0")) is True
    if not eligible:
        return "— nao elegivel", "secondary"
    if signed > 0:
        return "↑ aumento", "increase"
    if signed < 0:
        return "↓ reducao", "decrease"
    return "— sem mudanca", "neutral"


def _weight_table(rows: list[dict[str, str]], include_class: bool = True) -> str:
    if not rows:
        return '<p class="muted">Nenhuma conexao nesta categoria.</p>'
    body = []
    for row in rows:
        label, css_class = _weight_change_label(row)
        row_class = css_class if include_class else ""
        body.append(
            f'<tr class="connection-row {row_class}">'
            f'<td>{escaped(row["connection_id"])}</td><td>{escaped(row["source"])}</td>'
            f'<td>{escaped(row["target"])}</td><td>{escaped(row["source_type"])}</td>'
            f'<td>{escaped(row["target_type"])}</td><td>{escaped(row["delay"])}</td>'
            f'<td>{escaped(row["eligible"])}</td>'
            f'<td>{escaped(format_value("initial_weight", row["initial_weight"]))}</td>'
            f'<td>{escaped(format_value("final_weight", row["final_weight"]))}</td>'
            f'<td>{escaped(format_value("signed_change", row["signed_change"]))}</td>'
            f'<td>{escaped(format_value("absolute_change", row["absolute_change"]))}</td>'
            f'<td>{escaped(label)}</td></tr>'
        )
    return '<div class="table-wrap"><table><thead><tr><th>ID</th><th>Origem</th><th>Alvo</th><th>Tipo origem</th><th>Tipo alvo</th><th>Delay</th><th>Elegivel</th><th>Peso inicial</th><th>Peso final</th><th>Mudanca assinada</th><th>Mudanca absoluta</th><th>Estado</th></tr></thead><tbody>' + "".join(body) + "</tbody></table></div>"


def generate_weights_report(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    if not run_dir.is_dir():
        raise ReportGenerationError(f"pasta da execucao inexistente: {run_dir}")
    final_path = run_dir / "weights_final.csv"
    if not final_path.is_file():
        raise ReportGenerationError("arquivo obrigatorio ausente: weights_final.csv; esta execucao pode estar com STDP desligado")

    manifest = read_key_values(run_dir / "run_manifest.txt")
    metrics: dict[str, str] = {}
    if (run_dir / "metrics.csv").is_file():
        metrics.update(read_single_row_csv(run_dir / "metrics.csv"))
    if (run_dir / "plasticity_metrics.csv").is_file():
        metrics.update(read_single_row_csv(run_dir / "plasticity_metrics.csv"))
    if (run_dir / "homeostasis_metrics.csv").is_file():
        metrics.update(read_single_row_csv(run_dir / "homeostasis_metrics.csv"))
    if (run_dir / "reward_metrics.csv").is_file():
        metrics.update(read_single_row_csv(run_dir / "reward_metrics.csv"))
    weight_min = _decimal(manifest.get("plasticity_weight_min", ""))
    weight_max = _decimal(manifest.get("plasticity_weight_max", ""))
    streamed = _stream_weights(final_path, weight_min, weight_max)
    history = _history_summary(run_dir / "weight_history.csv")

    registered = int(streamed["count"])
    total_connections_text = _first(metrics, "network_total_connections", "connection_count") or manifest.get("total_connections") or manifest.get("num_connections")
    total_connections = _decimal(total_connections_text or "")
    eligible_text = _first(metrics, "plasticity_eligible_connection_count")
    eligible_total = _decimal(eligible_text or "")
    sampled = int(streamed["sampled_count"]) > 0
    if total_connections is not None and total_connections > registered:
        sampled = True
    if eligible_total is not None and eligible_total > int(streamed["eligible_count"]):
        sampled = True

    enabled = _flag(_first(metrics, "plasticity_enabled") or manifest.get("plasticity_enabled", "true"))
    cards = [
        ("Peso medio inicial", format_value("plasticity_initial_weight_mean", _first(metrics, "plasticity_initial_weight_mean") or "NA")),
        ("Peso medio final", format_value("plasticity_final_weight_mean", _first(metrics, "plasticity_final_weight_mean") or "NA")),
        ("Mudanca total assinada", format_value("plasticity_total_signed_change", _first(metrics, "plasticity_total_signed_change") or "NA")),
        ("Mudanca absoluta media", format_value("plasticity_mean_absolute_change", _first(metrics, "plasticity_mean_absolute_change") or "NA")),
        ("Conexoes modificadas", format_value("plasticity_modified_connection_count", _first(metrics, "plasticity_modified_connection_count", "plasticity_modified_connection_fraction") or "NA")),
        ("Eventos LTP", format_value("plasticity_potentiation_events", _first(metrics, "plasticity_potentiation_events") or "NA")),
        ("Eventos LTD", format_value("plasticity_depression_events", _first(metrics, "plasticity_depression_events") or "NA")),
        ("Clamps minimos", format_value("plasticity_clamp_min_events", _first(metrics, "plasticity_clamp_min_events") or "NA")),
        ("Clamps maximos", format_value("plasticity_clamp_max_events", _first(metrics, "plasticity_clamp_max_events") or "NA")),
    ]
    identification = [
        ("actual_run_name", _first(metrics, "actual_run_name", "run_name") or manifest.get("actual_run_name") or run_dir.name),
        ("topology", _first(metrics, "topology") or manifest.get("topology") or "NA"),
        ("plasticity_enabled", "ON" if enabled else "OFF"),
        ("plasticity_rule", _first(metrics, "plasticity_rule") or manifest.get("plasticity_rule") or "NA"),
        ("plasticity_learning_mode", _first(metrics, "plasticity_learning_mode") or manifest.get("plasticity_learning_mode") or "direct_stdp"),
        ("total_connections", total_connections_text or "NA"),
        ("eligible_connections", eligible_text or streamed["eligible_count"]),
        ("registered_connections", registered),
        ("sampled", "SIM" if sampled else "NAO"),
    ]

    body = _cards(cards)
    mechanisms = []
    learning_mode = str(identification[4][1])
    if learning_mode == "reward_modulated_stdp":
        mechanisms.append("Reward-Modulated STDP")
    elif enabled:
        mechanisms.append("Direct STDP")
    if _flag(_first(metrics, "homeostasis_synaptic_scaling_enabled") or manifest.get("homeostasis_synaptic_scaling_enabled", "false")):
        mechanisms.append("Synaptic scaling")
    if mechanisms:
        body += '<div class="notice">Mecanismos ativos: ' + escaped(", ".join(mechanisms)) + ". As estatisticas de cada mecanismo permanecem separadas.</div>"
    if _flag(_first(metrics, "homeostasis_enabled") or manifest.get("homeostasis_enabled", "false")):
        body += (
            '<div class="notice">Os deltas STDP e de scaling sao contabilizados '
            'separadamente. Os pesos finais representam o efeito liquido dos '
            'mecanismos ativos.</div>'
            + _key_value_table([
                ("homeostasis_scaling_events", _first(metrics, "homeostasis_scaling_events") or "NA"),
                ("homeostasis_scaling_total_signed_change", _first(metrics, "homeostasis_scaling_total_signed_change") or "NA"),
                ("homeostasis_scaling_total_absolute_change", _first(metrics, "homeostasis_scaling_total_absolute_change") or "NA"),
            ])
        )
    body += "<h2>Identificacao e cobertura</h2>" + _key_value_table(identification)
    if sampled:
        body += '<div class="notice">ATENCAO: a tabela abaixo representa uma amostra deterministica das conexoes. As metricas agregadas podem considerar todas as conexoes elegiveis.</div>'
    omitted = max(0, registered - WEIGHT_TABLE_LIMIT)
    if omitted:
        body += f'<div class="notice">Limite visual aplicado: {WEIGHT_TABLE_LIMIT} conexoes exibidas; {omitted} omitidas do HTML. Consulte weights_final.csv para os dados completos.</div>'

    body += "<h2>Tabela de conexoes</h2>" + _weight_table(streamed["displayed"])
    body += "<h2>Rankings e destaques</h2>"
    for title, key in (
        ("Maiores aumentos", "increases"),
        ("Maiores reducoes", "reductions"),
        ("Maiores mudancas absolutas", "absolute"),
        ("Conexoes sem alteracao", "unchanged"),
    ):
        body += f"<h3>{escaped(title)}</h3>" + _weight_table(streamed[key])
    body += "<h3>Conexoes no limite minimo</h3>" + _key_value_table([("count", streamed["min_count"]), ("weight_min", weight_min if weight_min is not None else "NA")])
    body += "<h3>Conexoes no limite maximo</h3>" + _key_value_table([("count", streamed["max_count"]), ("weight_max", weight_max if weight_max is not None else "NA")])

    if history is not None:
        body += "<h2>Historico de pesos</h2>" + _key_value_table(list(history.items()))
        body += _existing_file_links(run_dir, ("weight_history.csv", "plasticity_overview.png"))

    body += "<h2>Arquivos da execucao</h2>" + _existing_file_links(run_dir, (
        "weights_initial.csv", "weights_final.csv", "plasticity_metrics.csv",
        "weight_history.csv", "stdp_report.txt", "plasticity_overview.png",
        "metrics.csv", METRICS_REPORT_FILENAME, "run_manifest.txt",
        "reward_metrics.csv", "reward_connections.csv", REWARD_REPORT_FILENAME,
    ))
    body += _preview_images(run_dir, ("plasticity_overview.png", "reward_overview.png"))
    stdp_report = run_dir / "stdp_report.txt"
    if stdp_report.is_file():
        try:
            body += f"<h3>Relatorio STDP textual</h3><pre>{escaped(stdp_report.read_text(encoding='utf-8', errors='replace')[:100_000])}</pre>"
        except OSError:
            pass

    actual = str(identification[0][1])
    output = run_dir / WEIGHTS_REPORT_FILENAME
    _atomic_write(output, _document("MINISNN — RELATORIO DE PESOS E PLASTICIDADE", f"Execucao: {actual}", body))
    _update_manifest(run_dir)
    return output


def _read_csv_rows(path: Path, limit: int = 500) -> tuple[list[str], list[dict[str, str]]]:
    if not path.is_file():
        raise ReportGenerationError(f"arquivo obrigatorio ausente: {path.name}")
    rows: list[dict[str, str]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            headers = _validate_headers(path, reader.fieldnames)
            for line, raw in enumerate(reader, start=2):
                row = _validate_row(path, raw, line)
                if len(rows) < limit:
                    rows.append(row)
        return headers, rows
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(f"nao foi possivel ler {path.name}: {error}") from error


def _generic_table(headers: list[str], rows: list[dict[str, str]]) -> str:
    if not rows:
        return '<p class="muted">Nenhum registro nesta categoria.</p>'
    head = "".join(f"<th>{escaped(name)}</th>" for name in headers)
    body = "".join(
        "<tr>" + "".join(f"<td>{escaped(row.get(name, ''))}</td>" for name in headers) + "</tr>"
        for row in rows
    )
    return f'<div class="table-wrap"><table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table></div>'


def generate_reward_report(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    if not run_dir.is_dir():
        raise ReportGenerationError(f"pasta da execucao inexistente: {run_dir}")
    metrics = read_single_row_csv(run_dir / "reward_metrics.csv")
    if _flag(metrics.get("reward_enabled", "false")) is not True:
        raise ReportGenerationError("esta execucao nao possui reward ativo")
    manifest = read_key_values(run_dir / "run_manifest.txt")
    event_headers, events = _read_csv_rows(run_dir / "reward_events.csv")
    connection_headers, connections = _read_csv_rows(run_dir / "reward_connections.csv")

    cards = [
        ("Reward total", format_value("reward_cumulative_applied", metrics.get("reward_cumulative_applied", "NA"))),
        ("Reward positivo", format_value("reward_cumulative_positive", metrics.get("reward_cumulative_positive", "NA"))),
        ("Punicao total", format_value("reward_cumulative_negative", metrics.get("reward_cumulative_negative", "NA"))),
        ("Eventos", format_value("reward_event_count", metrics.get("reward_event_count", "NA"))),
        ("Elegibilidade media final", format_value("reward_eligibility_final_mean", metrics.get("reward_eligibility_final_mean", "NA"))),
        ("Conexoes modificadas", format_value("reward_modified_connection_count", metrics.get("reward_modified_connection_count", "NA"))),
        ("Mudanca total de peso", format_value("reward_weight_total_signed_change", metrics.get("reward_weight_total_signed_change", "NA"))),
        ("STDP mode", manifest.get("plasticity_learning_mode", "reward_modulated_stdp")),
        ("Homeostase", "ON" if _flag(manifest.get("homeostasis_enabled", "false")) else "OFF"),
    ]
    configuration_keys = (
        "reward_mode", "reward_learning_rate", "reward_eligibility_tau",
        "reward_eligibility_min", "reward_eligibility_max", "reward_signal_min",
        "reward_signal_max", "reward_clip_enabled",
    )
    eligibility_keys = tuple(key for key in metrics if "eligibility" in key)
    weight_keys = tuple(key for key in metrics if key.startswith("reward_weight_"))
    reward_keys = tuple(key for key in metrics if key.startswith("reward_cumulative_"))

    body = _cards(cards)
    body += "<h2>Resumo</h2><div class=\"notice\">O reward e um sinal escalar externo. A mudanca observada segue a regra R-STDP configurada e nao prova aprendizado de uma tarefa.</div>"
    body += "<h2>Configuracao</h2>" + _key_value_table([(key, metrics.get(key, "NA")) for key in configuration_keys])
    body += "<h2>Eventos</h2>" + _generic_table(event_headers, events)
    body += "<h2>Elegibilidade</h2>" + _key_value_table([(key, metrics[key]) for key in eligibility_keys])
    body += "<h2>Mudancas de peso</h2>" + _key_value_table([(key, metrics[key]) for key in weight_keys])
    body += "<h2>Recompensas e punicoes</h2>" + _key_value_table([(key, metrics[key]) for key in reward_keys])
    body += "<h2>Interacao com STDP</h2><p>A correlacao temporal gera elegibilidade; nao altera o peso imediatamente neste modo.</p>"
    body += "<h2>Interacao com homeostase</h2><p>R-STDP e aplicado antes do synaptic scaling. As estatisticas sao mantidas separadamente.</p>"
    body += "<h2>Rankings</h2>" + _generic_table(connection_headers, connections)
    body += "<h2>Historico e arquivos cientificos</h2>" + _existing_file_links(run_dir, (
        "reward_metrics.csv", "reward_events.csv", "reward_history.csv",
        "eligibility_history.csv", "reward_connections.csv", "reward_report.txt",
        "reward_overview.png", METRICS_REPORT_FILENAME, WEIGHTS_REPORT_FILENAME,
    ))
    body += _preview_images(run_dir, ("reward_overview.png",))
    body += "<h2>Limitacoes</h2><div class=\"notice\">Nao ha previsao de recompensa, politica, agente ou dopamina biologicamente detalhada. Punicao e reward negativo matematico.</div>"

    actual = manifest.get("actual_run_name", run_dir.name)
    output = run_dir / REWARD_REPORT_FILENAME
    _atomic_write(output, _document("MINISNN - RECOMPENSA, PUNICAO E ELEGIBILIDADE", f"Execucao: {actual}", body))
    _update_manifest(run_dir)
    return output


SCENARIO_HISTORY_COLUMNS = (
    "timestamp",
    "run_name",
    "actual_run_name",
    "run_path",
    "config_path",
    "topology",
    "num_neurons",
    "steps",
    "dt",
    "seed",
    "recorded_neuron",
    "total_connections",
    "total_spikes",
    "first_active_step",
    "last_active_step",
    "status",
)

SCENARIO_HISTORY_ARTIFACTS = (
    ("Metricas", "metrics_report.html"),
    ("Resumo", "summary.txt"),
    ("Manifesto", "run_manifest.txt"),
    ("Config usada", "config_used.ini"),
    ("Pesos", "weights_report.html"),
    ("STDP", "plasticity_overview.png"),
    ("Homeostase", "homeostasis_report.html"),
    ("Reward", "reward_report.html"),
)


def _read_scenario_history(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        raise ReportGenerationError(f"arquivo obrigatorio ausente: {path.name}")
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            headers = _validate_headers(path, reader.fieldnames)
            missing = [name for name in SCENARIO_HISTORY_COLUMNS if name not in headers]
            if missing:
                raise ReportGenerationError(
                    f"{path.name}: colunas obrigatorias ausentes: {', '.join(missing)}"
                )
            return [
                _validate_row(path, raw, line)
                for line, raw in enumerate(reader, start=2)
            ]
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(
            f"nao foi possivel ler {path.name}: {error}"
        ) from error


def _safe_history_run_directory(
    scenarios_directory: Path,
    raw_path: str,
) -> Path | None:
    text = raw_path.strip()
    if not text:
        return None

    root = scenarios_directory.resolve()
    source = Path(text)
    candidates = [source] if source.is_absolute() else [
        root / source,
        root.parent.parent / source,
    ]
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
            resolved.relative_to(root)
        except (OSError, ValueError):
            continue
        if resolved != root and resolved.is_dir():
            return resolved
    return None


def _history_artifact_links(
    scenarios_directory: Path,
    run_directory: Path | None,
) -> str:
    if run_directory is None:
        return '<span class="muted">arquivos indisponiveis</span>'

    relative_directory = run_directory.relative_to(scenarios_directory.resolve())
    relative_text = relative_directory.as_posix()
    links: list[str] = []
    for label, filename in SCENARIO_HISTORY_ARTIFACTS:
        if (run_directory / filename).is_file():
            href = quote(f"{relative_text}/{filename}", safe="/")
            links.append(
                f'<a href="{escaped(href)}">{escaped(label)}</a>'
            )
    links.append(
        f'<a href="{escaped(quote(relative_text + "/", safe="/"))}">Pasta</a>'
    )
    return '<span class="artifact-links">' + "".join(links) + "</span>"


def generate_scenario_history_report(
    scenarios_directory: Path | str,
    output_filename: str = HISTORY_REPORT_FILENAME,
) -> Path:
    scenarios_dir = Path(scenarios_directory)
    if not scenarios_dir.is_dir():
        raise ReportGenerationError(
            f"pasta de cenarios inexistente: {scenarios_dir}"
        )
    if (Path(output_filename).name != output_filename or
            not output_filename or
            Path(output_filename).suffix.lower() != ".html"):
        raise ReportGenerationError("nome de saida invalido")

    index_path = scenarios_dir / "index.csv"
    rows = _read_scenario_history(index_path)
    indexed_rows = list(enumerate(rows))
    indexed_rows.sort(
        key=lambda item: (item[1]["timestamp"], item[0]),
        reverse=True,
    )
    ordered_rows = [row for _, row in indexed_rows]

    ok_count = sum(row["status"].strip().upper() == "OK" for row in rows)
    run_names = {row["run_name"] for row in rows}
    topologies = {row["topology"] for row in rows}
    latest = ordered_rows[0]["timestamp"] if ordered_rows else "Nenhuma"
    body = _cards([
        ("Total de execucoes", len(rows)),
        ("Execucoes OK", ok_count),
        ("Execucoes com erro", len(rows) - ok_count),
        ("Run names distintos", len(run_names)),
        ("Topologias distintas", len(topologies)),
        ("Ultima execucao", latest),
    ])
    body += '<p><a href="index.csv">Abrir index.csv bruto</a></p>'

    if not ordered_rows:
        body += (
            '<div class="history-empty">Nenhuma execucao foi registrada ainda. '
            "Rode um cenario com history_enabled=true.</div>"
        )
    else:
        topology_options = "".join(
            f'<option value="{escaped(value)}">{escaped(value)}</option>'
            for value in sorted(topologies)
        )
        body += f"""
<div class="history-controls">
  <label>BUSCAR EXECUCOES<input id="history-search" type="search" placeholder="run, topologia, status ou data"></label>
  <label>STATUS<select id="history-status"><option value="">TODOS</option><option value="OK">OK</option><option value="ERRO">ERRO</option></select></label>
  <label>TOPOLOGIA<select id="history-topology"><option value="">TODAS</option>{topology_options}</select></label>
</div>
"""
        table_rows: list[str] = []
        for row in ordered_rows:
            status_ok = row["status"].strip().upper() == "OK"
            status_group = "OK" if status_ok else "ERRO"
            status_text = "OK" if status_ok else f'ERRO: {row["status"] or "status ausente"}'
            status_html = (
                f'<span class="status-ok">{escaped(status_text)}</span>'
                if status_ok else
                f'<span class="status-error">{escaped(status_text)}</span>'
            )
            search_text = " ".join(
                row[key] for key in
                ("run_name", "actual_run_name", "topology", "status", "timestamp")
            ).lower()
            run_directory = _safe_history_run_directory(
                scenarios_dir,
                row["run_path"],
            )
            artifacts = _history_artifact_links(scenarios_dir, run_directory)
            values = (
                row["timestamp"], row["run_name"], row["actual_run_name"],
                row["topology"], row["num_neurons"], row["total_connections"],
                row["steps"], row["dt"], row["seed"], row["total_spikes"],
                row["first_active_step"], row["last_active_step"],
            )
            cells = "".join(
                f'<td title="{escaped(value)}">{escaped(value)}</td>'
                for value in values
            )
            table_rows.append(
                '<tr class="history-row" '
                f'data-search="{escaped(search_text)}" '
                f'data-status="{status_group}" '
                f'data-topology="{escaped(row["topology"])}">'
                f"{cells}<td>{status_html}</td><td>{artifacts}</td></tr>"
            )
        body += """
<div class="table-wrap"><table id="history-table">
<thead><tr><th>Data/hora</th><th>Run solicitada</th><th>Run real</th><th>Topologia</th><th>Neuronios</th><th>Conexoes</th><th>Steps</th><th>dt</th><th>Seed</th><th>Spikes</th><th>Primeiro ativo</th><th>Ultimo ativo</th><th>Status</th><th>Arquivos</th></tr></thead>
<tbody>""" + "".join(table_rows) + """</tbody></table></div>
<script>
(() => {
  const search = document.getElementById('history-search');
  const status = document.getElementById('history-status');
  const topology = document.getElementById('history-topology');
  const rows = Array.from(document.querySelectorAll('.history-row'));
  const apply = () => {
    const query = search.value.trim().toLowerCase();
    for (const row of rows) {
      const matchesSearch = !query || row.dataset.search.includes(query);
      const matchesStatus = !status.value || row.dataset.status === status.value;
      const matchesTopology = !topology.value || row.dataset.topology === topology.value;
      row.hidden = !(matchesSearch && matchesStatus && matchesTopology);
    }
  };
  search.addEventListener('input', apply);
  status.addEventListener('change', apply);
  topology.addEventListener('change', apply);
})();
</script>
"""

    output = scenarios_dir / output_filename
    _atomic_write(
        output,
        _document(
            "MINISNN — HISTORICO DE EXECUCOES",
            "Fonte: index.csv append-only",
            body,
        ),
    )
    return output


EVOLUTION_HISTORY_COLUMNS = (
    "timestamp",
    "experiment_name",
    "actual_experiment_name",
    "experiment_path",
    "config_path",
    "base_scenario",
    "population_size",
    "generations",
    "gene_count",
    "best_fitness",
    "best_individual_id",
    "status",
)

EVOLUTION_HISTORY_ARTIFACTS = (
    ("Relatorio", EVOLUTION_REPORT_FILENAME),
    ("Grafico", "evolution_overview.png"),
    ("Manifesto", "evolution_manifest.txt"),
    ("Melhor genoma", "best_genome.csv"),
    ("Melhor execucao", "best_run/metrics_report.html"),
)


def _read_evolution_csv_rows(
    path: Path,
    required_columns: tuple[str, ...],
    *,
    allow_empty: bool = False,
) -> tuple[list[str], list[dict[str, str]]]:
    if not path.is_file():
        raise ReportGenerationError(f"arquivo obrigatorio ausente: {path.name}")
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            headers = _validate_headers(path, reader.fieldnames)
            missing = [name for name in required_columns if name not in headers]
            if missing:
                raise ReportGenerationError(
                    f"{path.name}: colunas obrigatorias ausentes: {', '.join(missing)}"
                )
            rows = [
                _validate_row(path, raw, line)
                for line, raw in enumerate(reader, start=2)
            ]
    except (OSError, UnicodeError, csv.Error) as error:
        raise ReportGenerationError(
            f"nao foi possivel ler {path.name}: {error}"
        ) from error
    if not rows and not allow_empty:
        raise ReportGenerationError(f"{path.name}: CSV vazio")
    return headers, rows


def generate_evolution_report(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    if not run_dir.is_dir():
        raise ReportGenerationError(f"pasta evolutiva inexistente: {run_dir}")

    generation_headers, generations = _read_evolution_csv_rows(
        run_dir / "generations.csv",
        (
            "generation", "population_size", "fitness_best", "fitness_mean",
            "fitness_min", "fitness_max", "global_best_fitness",
            "best_individual_id", "global_best_individual_id",
            "diversity_mean_gene_std", "mutation_count",
        ),
    )
    genome_headers, best_genome = _read_evolution_csv_rows(
        run_dir / "best_genome.csv",
        ("individual_id", "generation", "fitness_selection", "gene_index",
         "gene_name", "gene_kind", "value", "minimum", "maximum"),
    )
    lineage_headers, lineage = _read_evolution_csv_rows(
        run_dir / "lineage.csv",
        ("child_generation", "child_individual_id", "operation"),
        allow_empty=True,
    )
    manifest = read_key_values(run_dir / "evolution_manifest.txt")
    evolution_config = read_key_values(run_dir / "evolution_config_used.ini")
    base_config = read_key_values(run_dir / "base_scenario_used.ini")

    initial = _decimal(generations[0]["fitness_best"])
    best = max(
        (_decimal(row["global_best_fitness"]) for row in generations),
        key=lambda value: value if value is not None else Decimal("-Infinity"),
    )
    if initial is None or best is None:
        raise ReportGenerationError("generations.csv: fitness invalido")
    improvement = best - initial
    improvement_percent = (
        improvement / abs(initial) * Decimal("100") if initial != 0 else None
    )
    planned_generations = _decimal(evolution_config.get("generations", ""))
    completed_generations = len(generations)
    status = "COMPLETED"
    if planned_generations is not None and completed_generations < int(planned_generations):
        status = "CHECKPOINTED"

    body = _cards([
        ("Melhor fitness", format_value("best_fitness", best)),
        ("Fitness inicial", format_value("initial_fitness", initial)),
        ("Melhora absoluta", format_value("improvement", improvement)),
        ("Melhora percentual", f"{improvement_percent:.2f}%" if improvement_percent is not None else "NA"),
        ("Populacao", generations[-1]["population_size"]),
        ("Geracoes", completed_generations),
        ("Genes", len(best_genome)),
        ("Replicas", manifest.get("replicates", "NA")),
        ("Melhor individuo", generations[-1]["global_best_individual_id"]),
        ("Status", status),
    ])
    body += (
        "<h2>Resumo</h2><p>O genoma foi selecionado segundo a funcao de fitness "
        "configurada. O resultado e especifico ao cenario, aos limites e as seeds.</p>"
    )
    body += "<h2>Configuracao</h2>" + _key_value_table(
        [(key, value) for key, value in evolution_config.items()
         if key != "base_scenario"]
    )
    body += "<h2>Cenario-base</h2>" + _key_value_table(list(base_config.items()))
    body += "<h2>Genoma</h2>" + _generic_table(genome_headers, best_genome[:500])
    body += (
        "<h2>Fitness</h2><p>Fitness de selecao agrega termos ponderados e, quando "
        "configurado, penaliza variacao entre replicas.</p>"
    )
    generation_columns = [
        name for name in (
            "generation", "fitness_best", "fitness_mean", "fitness_min",
            "fitness_max", "global_best_fitness", "diversity_mean_gene_std",
            "diversity_mean_pair_distance", "mutation_count",
        ) if name in generation_headers
    ]
    body += "<h2>Geracoes</h2>" + _generic_table(
        generation_columns,
        [{name: row[name] for name in generation_columns} for row in generations],
    )
    body += "<h2>Melhor individuo</h2>" + _key_value_table([
        ("individual_id", generations[-1]["global_best_individual_id"]),
        ("fitness", best),
        ("generation", best_genome[0]["generation"]),
    ])
    body += "<h2>Genes do melhor</h2>" + _generic_table(genome_headers, best_genome[:500])
    body += "<h2>Linhagem</h2>" + (
        _generic_table(lineage_headers, lineage[-500:])
        if lineage else '<p class="muted">Nenhuma linha de linhagem disponivel.</p>'
    )
    body += (
        "<h2>Diversidade</h2><p>As medidas registradas descrevem dispersao dos "
        "genes na populacao; valor baixo nao prova convergencia global.</p>"
        "<h2>Plasticidade durante a vida</h2><p>Alteracoes aprendidas durante uma "
        "avaliacao afetam o fitness, mas nao substituem o genoma inicial. A heranca "
        "desta versao e darwiniana.</p>"
        "<h2>Checkpoints</h2><p>O checkpoint textual e escrito em fronteiras de "
        "geracao e preserva o estado do PRNG.</p>"
    )
    body += "<h2>Arquivos cientificos</h2>" + _existing_file_links(run_dir, (
        "generations.csv", "individuals.csv", "replicates.csv",
        "fitness_terms.csv", "genomes.csv", "lineage.csv", "best_genome.csv",
        "best_network_initial.csv", "evolution_overview.png",
        "evolution_manifest.txt", "evolution_report.txt",
        "best_run/metrics_report.html",
    ))
    body += _preview_images(run_dir, ("evolution_overview.png",))
    body += (
        '<h2>Limitacoes</h2><div class="notice">Melhor fitness nao significa '
        "inteligencia geral nem otimo global. A topologia e os delays permanecem "
        "fixos, a diversidade pode colapsar e a avaliacao e serial.</div>"
    )

    actual_name = manifest.get("actual_experiment_name", run_dir.name)
    output = run_dir / EVOLUTION_REPORT_FILENAME
    _atomic_write(output, _document(
        "MINISNN - RELATORIO DE NEUROEVOLUCAO",
        f"Experimento: {actual_name}",
        body,
    ))
    return output


def generate_evolution_history_report(
    evolution_directory: Path | str,
    output_filename: str = EVOLUTION_HISTORY_FILENAME,
) -> Path:
    evolution_dir = Path(evolution_directory)
    if not evolution_dir.is_dir():
        raise ReportGenerationError(
            f"pasta de evolucao inexistente: {evolution_dir}"
        )
    if (Path(output_filename).name != output_filename or not output_filename or
            Path(output_filename).suffix.lower() != ".html"):
        raise ReportGenerationError("nome de saida invalido")

    _, rows = _read_evolution_csv_rows(
        evolution_dir / "index.csv",
        EVOLUTION_HISTORY_COLUMNS,
        allow_empty=True,
    )
    indexed_rows = list(enumerate(rows))
    indexed_rows.sort(key=lambda item: (item[1]["timestamp"], item[0]), reverse=True)
    ordered_rows = [row for _, row in indexed_rows]
    ok_count = sum(row["status"].strip().upper() == "OK" for row in rows)
    body = _cards([
        ("Experimentos", len(rows)),
        ("Concluidos", ok_count),
        ("Outros status", len(rows) - ok_count),
        ("Nomes distintos", len({row["experiment_name"] for row in rows})),
        ("Ultimo registro", ordered_rows[0]["timestamp"] if ordered_rows else "Nenhum"),
    ])
    body += '<p><a href="index.csv">Abrir index.csv bruto</a></p>'
    if not ordered_rows:
        body += '<div class="history-empty">Nenhum experimento evolutivo registrado.</div>'
    else:
        body += """
<div class="history-controls">
  <label>BUSCAR EXPERIMENTOS<input id="history-search" type="search" placeholder="nome, cenario, status ou data"></label>
  <label>STATUS<select id="history-status"><option value="">TODOS</option><option value="OK">OK</option><option value="ERRO">ERRO</option></select></label>
</div>
"""
        table_rows: list[str] = []
        for row in ordered_rows:
            status_ok = row["status"].strip().upper() == "OK"
            status_group = "OK" if status_ok else "ERRO"
            status_text = "OK" if status_ok else f'ERRO: {row["status"] or "status ausente"}'
            status_html = (
                f'<span class="status-ok">{escaped(status_text)}</span>'
                if status_ok else
                f'<span class="status-error">{escaped(status_text)}</span>'
            )
            run_directory = _safe_history_run_directory(
                evolution_dir, row["experiment_path"]
            )
            artifacts = _evolution_history_links(evolution_dir, run_directory)
            search_text = " ".join(
                row[key] for key in (
                    "experiment_name", "actual_experiment_name", "base_scenario",
                    "status", "timestamp",
                )
            ).lower()
            values = (
                row["timestamp"], row["experiment_name"],
                row["actual_experiment_name"], Path(row["base_scenario"]).name,
                row["population_size"], row["generations"], row["gene_count"],
                row["best_fitness"], row["best_individual_id"],
            )
            cells = "".join(f"<td>{escaped(value)}</td>" for value in values)
            table_rows.append(
                '<tr class="history-row" '
                f'data-search="{escaped(search_text)}" data-status="{status_group}">'
                f"{cells}<td>{status_html}</td><td>{artifacts}</td></tr>"
            )
        body += """
<div class="table-wrap"><table><thead><tr><th>Data/hora</th><th>Experimento</th><th>Nome real</th><th>Cenario-base</th><th>Populacao</th><th>Geracoes</th><th>Genes</th><th>Melhor fitness</th><th>Melhor individuo</th><th>Status</th><th>Arquivos</th></tr></thead><tbody>""" + "".join(table_rows) + """</tbody></table></div>
<script>
(() => {
  const search = document.getElementById('history-search');
  const status = document.getElementById('history-status');
  const rows = Array.from(document.querySelectorAll('.history-row'));
  const apply = () => {
    const query = search.value.trim().toLowerCase();
    for (const row of rows) {
      row.hidden = !((!query || row.dataset.search.includes(query)) &&
                     (!status.value || row.dataset.status === status.value));
    }
  };
  search.addEventListener('input', apply);
  status.addEventListener('change', apply);
})();
</script>"""

    output = evolution_dir / output_filename
    _atomic_write(output, _document(
        "MINISNN - HISTORICO EVOLUTIVO",
        "Fonte: index.csv append-only",
        body,
    ))
    return output


def _evolution_history_links(
    evolution_directory: Path,
    run_directory: Path | None,
) -> str:
    if run_directory is None:
        return '<span class="muted">arquivos indisponiveis</span>'
    relative = run_directory.relative_to(evolution_directory.resolve()).as_posix()
    links: list[str] = []
    for label, filename in EVOLUTION_HISTORY_ARTIFACTS:
        if (run_directory / filename).is_file():
            href = quote(f"{relative}/{filename}", safe="/")
            links.append(f'<a href="{escaped(href)}">{escaped(label)}</a>')
    links.append(f'<a href="{escaped(quote(relative + "/", safe="/"))}">Pasta</a>')
    return '<span class="artifact-links">' + "".join(links) + "</span>"
