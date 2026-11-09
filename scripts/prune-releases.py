#!/usr/bin/env python3
"""Keep one current public release. Dry run unless --apply is specified."""
import argparse
import json
import re
import subprocess

VERSION = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")


def api(path, method="GET"):
    result = subprocess.run(["gh", "api", "--method", method, path],
                            check=True, capture_output=True, text=True, encoding="utf-8")
    return json.loads(result.stdout) if result.stdout.strip() else None


def version(tag):
    match = VERSION.fullmatch(tag)
    return tuple(map(int, match.groups())) if match else None


def published_stable(release):
    return bool(release.get("published_at")) and not release["draft"] and not release["prerelease"]


def identity(release):
    return release["id"], release["tag_name"], release["published_at"]


def candidates(releases, latest):
    newest = version(latest["tag_name"])
    older = []
    for release in releases:
        if not published_stable(release) or release["id"] == latest["id"]:
            continue
        tag = release["tag_name"]
        number = version(tag)
        if number is None:
            continue
        if number >= newest or release["published_at"] >= latest["published_at"]:
            raise RuntimeError(f"Stable release {tag} is not older than verified latest; refusing cleanup")
        older.append(release)
    return older


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--expected-latest", required=True, help="New release whose anonymous downloads were verified")
    parser.add_argument("--expected-latest-id", type=int, help="GitHub ID of the verified release")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--delete-tags", action="store_true")
    args = parser.parse_args()
    if (not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo) or not version(args.expected_latest)
            or (args.expected_latest_id is not None and args.expected_latest_id < 1)):
        parser.error("Invalid repository or release tag")
    base = "repos/" + args.repo

    latest = api(base + "/releases/latest")
    if (not published_stable(latest) or latest["tag_name"] != args.expected_latest
            or (args.expected_latest_id is not None and latest["id"] != args.expected_latest_id)):
        raise RuntimeError("Latest release does not match the verified release; refusing cleanup")

    def check_latest():
        current = api(base + "/releases/latest")
        if not published_stable(current) or identity(current) != identity(latest):
            raise RuntimeError("Latest release changed; refusing cleanup")

    def check_release(release):
        check_latest()
        current = api(base + "/releases/" + str(release["id"]))
        if not published_stable(current) or identity(current) != identity(release):
            raise RuntimeError(f"Release {release['tag_name']} changed during cleanup")
        return current

    def tag_target(tag):
        # Matching refs returns a list even when the exact tag is absent.
        refs = api(base + "/git/matching-refs/tags/" + tag)
        return next((item["object"] for item in refs if item["ref"] == "refs/tags/" + tag), None)

    # Explicit pagination avoids silently retaining releases beyond the first page.
    def pages(path):
        result = []
        for page in range(1, 101):
            batch = api(base + path + f"?per_page=100&page={page}")
            result.extend(batch)
            if len(batch) < 100:
                return result
        raise RuntimeError("Repository exceeds cleanup bounds")

    def shared_tag(release, releases):
        return any(item["id"] != release["id"] and item["tag_name"] == release["tag_name"] for item in releases)

    releases = pages("/releases")
    plan = []
    for release in candidates(releases, latest):
        tag = release["tag_name"]
        clean_tag = args.delete_tags and not shared_tag(release, releases)
        target = tag_target(tag) if clean_tag else None
        if target and release.get("immutable"):
            raise RuntimeError(f"Immutable release {tag} locks its tag; refusing tag-first cleanup")
        plan.append((release, clean_tag, target))

    action = "Delete" if args.apply else "Would delete"
    for release, clean_tag, target in plan:
        tag = release["tag_name"]
        release_path = base + "/releases/" + str(release["id"])
        if args.apply:
            current_releases = pages("/releases")
            candidates(current_releases, latest)
        if clean_tag:
            if args.apply:
                current = check_release(release)
                if shared_tag(release, current_releases):
                    clean_tag = False
                elif tag_target(tag) != target or (target and current.get("immutable")):
                    raise RuntimeError(f"Tag {tag} changed or was locked during cleanup")
            if clean_tag and target:
                print(f"{action} old release tag: {tag}")
                if args.apply:
                    api(base + "/git/refs/tags/" + tag, "DELETE")
        print(f"{action} old release: {tag}")
        if args.apply:
            check_release(release)
            # Keep the release record if its tag reappears so cleanup can be retried.
            if clean_tag and tag_target(tag) is not None:
                raise RuntimeError(f"Tag {tag} reappeared during cleanup")
            api(release_path, "DELETE")


if __name__ == "__main__":
    main()
