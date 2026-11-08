#!/usr/bin/env python3
"""Keep one current public release. Dry run unless --apply is specified."""
import argparse
import json
import re
import subprocess

VERSION = re.compile(r"^v\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")


def api(path, method="GET"):
    result = subprocess.run(["gh", "api", "--method", method, path],
                            check=True, capture_output=True, text=True, encoding="utf-8")
    return json.loads(result.stdout) if result.stdout.strip() else None


def candidates(releases, tags, latest):
    if not latest or latest.get("draft") or latest.get("prerelease") or not VERSION.fullmatch(latest.get("tag_name", "")):
        raise ValueError("A stable public latest release is required")
    keep = {latest["tag_name"]} | {item["tag_name"] for item in releases if item.get("draft") or item.get("prerelease")}
    old_releases = [item for item in releases if item["tag_name"] not in keep and VERSION.fullmatch(item["tag_name"])]
    old_tags = [item["name"] for item in tags if item["name"] not in keep and VERSION.fullmatch(item["name"])]
    return old_releases, old_tags


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--expected-latest", required=True, help="New release whose anonymous downloads were verified")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--delete-tags", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo) or not VERSION.fullmatch(args.expected_latest):
        parser.error("Invalid repository or release tag")
    base = "repos/" + args.repo

    def check_latest():
        latest = api(base + "/releases/latest")
        if latest["tag_name"] != args.expected_latest or latest.get("draft") or latest.get("prerelease"):
            raise RuntimeError("Latest release changed; refusing cleanup")
        return latest

    # Explicit pagination avoids silently retaining releases beyond the first page.
    def pages(path):
        result = []
        for page in range(1, 101):
            batch = api(base + path + f"?per_page=100&page={page}")
            result.extend(batch)
            if len(batch) < 100:
                return result
        raise RuntimeError("Repository exceeds cleanup bounds")

    latest = check_latest()
    releases, tags = candidates(pages("/releases"), pages("/tags"), latest)
    for release in releases:
        print(("Delete" if args.apply else "Would delete") + " old release: " + release["tag_name"])
        if args.apply:
            check_latest()
            current = api(base + "/releases/" + str(release["id"]))
            if current.get("draft") or current.get("prerelease") or current["tag_name"] == args.expected_latest:
                raise RuntimeError("Release changed during cleanup")
            api(base + "/releases/" + str(release["id"]), "DELETE")
    if args.delete_tags:
        for tag in tags:
            print(("Delete" if args.apply else "Would delete") + " old version tag: " + tag)
            if args.apply:
                check_latest()
                # A newly created draft/prerelease must keep its tag.
                protected = {item["tag_name"] for item in pages("/releases") if item.get("draft") or item.get("prerelease")}
                if tag in protected:
                    continue
                api(base + "/git/refs/tags/" + tag, "DELETE")


if __name__ == "__main__":
    main()
