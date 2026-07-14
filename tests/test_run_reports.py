from __future__ import annotations

from pathlib import Path
import csv
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

from html_report_common import (  # noqa: E402
    ReportGenerationError,
    WEIGHT_TABLE_LIMIT,
    generate_metrics_report,
    generate_weights_report,
)


TEMP_ROOT = PROJECT_ROOT / "build" / "HTML Report Tests With Spaces"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_single(path: Path, values: dict[str, object]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(values))
        writer.writeheader()
        writer.writerow(values)


def write_metrics(run: Path, **updates: object) -> None:
    values: dict[str, object] = {
        "run_name": "basic_run",
        "actual_run_name": "basic_run",
        "run_path": r"C:\Users\example\miniSNN\results\scenarios\basic_run",
        "timestamp": "20260713_120000",
        "topology": "chain",
        "run_seed": 7,
        "diagnostics_level": "basic",
        "network_num_neurons": 5,
        "network_total_connections": 4,
        "activity_total_spikes": 42,
        "activity_fraction": 0.423,
        "activity_first_active_step": 2,
        "activity_last_active_step": 98,
        "diagnostic_regime": "sustained",
        "diagnostic_stability_score": 0.75,
        "plasticity_enabled": "false",
    }
    values.update(updates)
    write_single(run / "metrics.csv", values)


WEIGHT_FIELDS = [
    "connection_id", "source", "target", "source_type", "target_type",
    "delay", "weight", "eligible", "sampled", "initial_weight",
    "final_weight", "signed_change", "absolute_change",
]


def write_weights(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=WEIGHT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def weight_row(
    connection_id: int,
    change: float,
    *,
    eligible: int = 1,
    sampled: int = 0,
) -> dict[str, object]:
    initial = 100.0
    final = initial + change
    return {
        "connection_id": connection_id,
        "source": connection_id % 4,
        "target": (connection_id + 1) % 4,
        "source_type": "EXC" if eligible else "INH",
        "target_type": "EXC",
        "delay": 1,
        "weight": final,
        "eligible": eligible,
        "sampled": sampled,
        "initial_weight": initial if eligible else -180.0,
        "final_weight": final if eligible else -180.0,
        "signed_change": change if eligible else 0.0,
        "absolute_change": abs(change) if eligible else 0.0,
    }


def expect_failure(callback, output: Path, message: str) -> None:
    output.unlink(missing_ok=True)
    try:
        callback()
    except ReportGenerationError as error:
        check(str(error).strip() != "", message + " clear message")
    else:
        raise AssertionError(message)
    check(not output.exists(), message + " partial output")


def main() -> int:
    shutil.rmtree(TEMP_ROOT, ignore_errors=True)
    TEMP_ROOT.mkdir(parents=True)
    try:
        basic = TEMP_ROOT / "Basic Legacy Run"
        basic.mkdir()
        write_metrics(basic)
        (basic / "population.csv").write_text("tempo,spikes_total\n0,0\n", encoding="utf-8")
        metrics_before = (basic / "metrics.csv").read_bytes()
        output = generate_metrics_report(basic)
        text = output.read_text(encoding="utf-8")
        check((basic / "metrics.csv").read_bytes() == metrics_before, "metrics CSV changed")
        check(output.stat().st_size > 0, "basic report is empty")
        check("RELATORIO DE METRICAS" in text, "metrics title")
        check("Atividade populacional" in text, "metrics sections")
        check("0.423 (42.30%)" in text, "fraction formatting")
        check('href="metrics.csv"' in text, "relative raw link")
        check("http://" not in text and "https://" not in text, "external URL")
        check("C:\\Users" not in text, "absolute run path leaked")
        check("Plasticidade: DESATIVADA" in text, "STDP off presentation")

        stdp = TEMP_ROOT / "STDP Run"
        stdp.mkdir()
        write_metrics(stdp, plasticity_enabled="true")
        write_single(
            stdp / "plasticity_metrics.csv",
            {
                "plasticity_enabled": "true",
                "plasticity_rule": "stdp_pair_trace",
                "plasticity_eligible_connection_count": 4,
                "plasticity_modified_connection_count": 3,
                "plasticity_modified_connection_fraction": 0.75,
                "plasticity_initial_weight_mean": 100,
                "plasticity_final_weight_mean": 101.25,
                "plasticity_mean_absolute_change": 1.5,
                "plasticity_total_signed_change": 5,
                "plasticity_potentiation_events": 8,
                "plasticity_depression_events": 3,
                "plasticity_clamp_min_events": 1,
                "plasticity_clamp_max_events": 2,
            },
        )
        stdp_text = generate_metrics_report(stdp).read_text(encoding="utf-8")
        check("stdp_pair_trace" in stdp_text, "STDP rule")
        check("plasticity_potentiation_events" in stdp_text, "STDP metrics")

        home = TEMP_ROOT / "Partial Homeostasis Run"
        home.mkdir()
        write_metrics(home, homeostasis_enabled="true")
        write_single(
            home / "homeostasis_metrics.csv",
            {
                "homeostasis_enabled": "true",
                "homeostasis_intrinsic_enabled": "true",
                "homeostasis_synaptic_scaling_enabled": "false",
                "homeostasis_inhibitory_gain_enabled": "false",
                "homeostasis_target_rate": 0.05,
                "homeostasis_population_rate_final": 0.04,
                "homeostasis_rate_error_final": -0.01,
                "homeostasis_threshold_final_mean": -51.0,
                "homeostasis_inhibitory_gain_final": 1.0,
            },
        )
        (home / "homeostasis_report.txt").write_text(
            "Resumo <script>alert('home')</script>", encoding="utf-8"
        )
        home_text = generate_metrics_report(home).read_text(encoding="utf-8")
        check("13. Homeostase" in home_text, "homeostasis metrics section")
        check("Mecanismos homeostaticos simplificados" in home_text,
              "homeostasis limitation notice")
        check('href="homeostasis_metrics.csv"' in home_text,
              "homeostasis relative CSV link")
        check("&lt;script&gt;" in home_text and "<script>alert('home')</script>" not in home_text,
              "escaped homeostasis report")
        check("http://" not in home_text and "https://" not in home_text,
              "homeostasis report external resource")

        escaped_run = TEMP_ROOT / "Escaped Run"
        escaped_run.mkdir()
        write_metrics(
            escaped_run,
            run_name="<script>alert(1)</script>",
            actual_run_name="<script>alert(1)</script>",
        )
        escaped_text = generate_metrics_report(escaped_run).read_text(encoding="utf-8")
        check("&lt;script&gt;alert(1)&lt;/script&gt;" in escaped_text, "escaped run name")
        check("<script>alert(1)</script>" not in escaped_text, "script injection")

        write_weights(
            stdp / "weights_final.csv",
            [
                weight_row(0, 2.5, sampled=1),
                weight_row(1, -3.25, sampled=1),
                weight_row(2, 0.0, sampled=1),
                weight_row(3, 0.0, eligible=0, sampled=1),
            ],
        )
        (stdp / "weights_initial.csv").write_text("connection_id\n0\n", encoding="utf-8")
        (stdp / "weight_history.csv").write_text(
            "step,connection_id,source,target,weight\n0,0,0,1,100\n10,0,0,1,102.5\n",
            encoding="utf-8",
        )
        (stdp / "run_manifest.txt").write_text(
            "actual_run_name=stdp_run\ntopology=chain\nplasticity_enabled=true\n"
            "plasticity_rule=stdp_pair_trace\nplasticity_weight_min=0\n"
            "plasticity_weight_max=200\nhomeostasis_enabled=true\n",
            encoding="utf-8",
        )
        write_single(
            stdp / "homeostasis_metrics.csv",
            {
                "homeostasis_enabled": "true",
                "homeostasis_synaptic_scaling_enabled": "true",
                "homeostasis_scaling_events": 3,
                "homeostasis_scaling_total_signed_change": -1.25,
                "homeostasis_scaling_total_absolute_change": 4.5,
            },
        )
        weights_before = (stdp / "weights_final.csv").read_bytes()
        weight_output = generate_weights_report(stdp)
        weight_text = weight_output.read_text(encoding="utf-8")
        check(
            (stdp / "weights_final.csv").read_bytes() == weights_before,
            "weights CSV changed",
        )
        check("↑ aumento" in weight_text, "increase label")
        check("↓ reducao" in weight_text, "decrease label")
        check("sem mudanca" in weight_text, "unchanged label")
        check("nao elegivel" in weight_text, "ineligible label")
        check("Maiores aumentos" in weight_text, "rankings")
        check("amostra deterministica" in weight_text, "sampling warning")
        check("Historico de pesos" in weight_text, "history summary")
        check('href="weights_final.csv"' in weight_text, "complete CSV link")
        check("deltas STDP e de scaling" in weight_text,
              "STDP and scaling distinction")

        large = TEMP_ROOT / "Large Weight Run"
        large.mkdir()
        write_weights(
            large / "weights_final.csv",
            [weight_row(index, (index % 7) - 3) for index in range(WEIGHT_TABLE_LIMIT + 25)],
        )
        large_text = generate_weights_report(large).read_text(encoding="utf-8")
        check("25 omitidas do HTML" in large_text, "visual omission notice")
        check("Limite visual aplicado" in large_text, "visual limit notice")

        invalid = TEMP_ROOT / "Invalid Inputs"
        invalid.mkdir()
        expect_failure(
            lambda: generate_metrics_report(invalid),
            invalid / "metrics_report.html",
            "missing metrics",
        )
        (invalid / "metrics.csv").write_text("", encoding="utf-8")
        expect_failure(
            lambda: generate_metrics_report(invalid),
            invalid / "metrics_report.html",
            "empty metrics",
        )
        (invalid / "metrics.csv").write_text("run_name,total_spikes\nbad,nan\n", encoding="utf-8")
        expect_failure(
            lambda: generate_metrics_report(invalid),
            invalid / "metrics_report.html",
            "nonfinite metrics",
        )
        (invalid / "metrics.csv").write_text("run_name,total_spikes\nbad\n", encoding="utf-8")
        expect_failure(
            lambda: generate_metrics_report(invalid),
            invalid / "metrics_report.html",
            "incomplete metrics",
        )
        write_metrics(invalid, homeostasis_enabled="true")
        (invalid / "homeostasis_metrics.csv").write_text(
            "homeostasis_enabled,homeostasis_population_rate_final\ntrue,nan\n",
            encoding="utf-8",
        )
        expect_failure(
            lambda: generate_metrics_report(invalid),
            invalid / "metrics_report.html",
            "nonfinite homeostasis metrics",
        )
        expect_failure(
            lambda: generate_weights_report(invalid),
            invalid / "weights_report.html",
            "missing weights",
        )

        command = [
            sys.executable,
            str(SCRIPTS_DIR / "generate_run_reports.py"),
            str(invalid),
            "--weights",
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        check(result.returncode != 0, "invalid CLI exit code")
        check("Erro:" in result.stdout, "invalid CLI message")
        check("Traceback" not in result.stdout + result.stderr, "confusing traceback")

        print("HTML run reports validation OK")
        return 0
    finally:
        shutil.rmtree(TEMP_ROOT, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
