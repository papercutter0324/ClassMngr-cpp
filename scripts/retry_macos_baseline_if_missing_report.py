#!/usr/bin/env python3
"""Retry a failed macOS baseline only when the run published no result report."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request

MACOS_JOB_NAME = "macOS universal Debug"
MACOS_REPORT_NAME = "refactoring-baseline-macos-clang-debug"
GITHUB_API_VERSION = "2022-11-28"


def failed_macos_job_without_report(
    jobs: list[dict[str, object]], artifacts: list[dict[str, object]]
) -> dict[str, object] | None:
    """Return the failed macOS job only when its report artifact is unavailable."""
    macos_job = next(
        (job for job in jobs if job.get("name") == MACOS_JOB_NAME), None
    )
    if macos_job is None or macos_job.get("conclusion") != "failure":
        return None

    report_published = any(
        artifact.get("name") == MACOS_REPORT_NAME and not artifact.get("expired")
        for artifact in artifacts
    )
    return None if report_published else macos_job


def request_json(
    api_url: str,
    repository: str,
    token: str,
    method: str,
    path: str,
    body: dict[str, object] | None = None,
) -> dict[str, object]:
    data = json.dumps(body).encode("utf-8") if body is not None else None
    request = urllib.request.Request(
        f"{api_url.rstrip('/')}/repos/{repository}{path}",
        data=data,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "X-GitHub-Api-Version": GITHUB_API_VERSION,
        },
    )
    with urllib.request.urlopen(request, timeout=20) as response:
        payload = response.read()
    return json.loads(payload) if payload else {}


def main() -> int:
    if os.environ.get("GH_RUN_ATTEMPT") != "1":
        print("Automatic macOS baseline retry is limited to the first run attempt.")
        return 0

    api_url = os.environ["GH_API_URL"]
    repository = os.environ["GH_REPOSITORY"]
    run_id = os.environ["GH_RUN_ID"]
    token = os.environ["GH_TOKEN"]

    try:
        jobs_response = request_json(
            api_url,
            repository,
            token,
            "GET",
            f"/actions/runs/{run_id}/jobs?filter=latest&per_page=100",
        )
        artifacts_response = request_json(
            api_url,
            repository,
            token,
            "GET",
            f"/actions/runs/{run_id}/artifacts?per_page=100",
        )
    except urllib.error.HTTPError as error:
        if error.code in (403, 404):
            print(
                "GitHub did not authorize inspection of the baseline run. "
                "The failed baseline remains visible for manual review."
            )
            return 0
        raise

    jobs = jobs_response.get("jobs", [])
    artifacts = artifacts_response.get("artifacts", [])
    if not isinstance(jobs, list) or not isinstance(artifacts, list):
        raise RuntimeError("GitHub returned an invalid jobs or artifacts response.")

    macos_job = failed_macos_job_without_report(jobs, artifacts)
    if macos_job is None:
        print(
            "The macOS job succeeded, did not fail, or published its result "
            "artifact; preserving the original workflow outcome without retry."
        )
        return 0

    job_id = macos_job.get("id")
    if not isinstance(job_id, int):
        raise RuntimeError("The failed macOS job response did not include a job id.")

    print(
        "The macOS job failed without publishing its result artifact. "
        "Requesting its single automatic rerun with runner diagnostics."
    )
    try:
        request_json(
            api_url,
            repository,
            token,
            "POST",
            f"/actions/jobs/{job_id}/rerun",
            {"enable_debug_logging": True},
        )
    except urllib.error.HTTPError as error:
        if error.code in (403, 404):
            print(
                "GitHub did not authorize the automatic rerun. "
                "The failed baseline remains visible for manual review."
            )
            return 0
        raise

    print(f"Queued macOS baseline rerun for job {job_id}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
