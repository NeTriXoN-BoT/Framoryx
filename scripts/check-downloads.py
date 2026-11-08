"""Verify the public latest-release buttons without credentials or browser cookies."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import time
from urllib import error, parse, request

ALLOWED_HOSTS = {"github.com", "api.github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com"}
ASSET = "Framoryx-Windows-x64.zip"
ALL_ASSETS = (ASSET, "Framoryx-Linux-x86_64.AppImage", "Framoryx-Linux-x86_64.tar.gz", "Framoryx-Source.zip")


class PublicRedirects(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        validate_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def validate_url(url):
    parsed = parse.urlsplit(url)
    if parsed.scheme != "https" or parsed.hostname not in ALLOWED_HOSTS or parsed.username or parsed.password:
        raise RuntimeError("Unexpected download destination")


def canonical_route(url):
    parsed = parse.urlsplit(url)
    parts = parsed.path.split("/")
    if parsed.netloc.lower() == "github.com" and len(parts) >= 3:
        parts[1:3] = [part.lower() for part in parts[1:3]]
        return parsed._replace(netloc="github.com", path="/".join(parts)).geturl()
    return url


def download(url, limit):
    validate_url(url)
    # No password manager, netrc, Authorization header, token, or cookie jar is installed.
    opener = request.build_opener(PublicRedirects())
    for attempt in range(3):
        try:
            req = request.Request(url, headers={"User-Agent": "Framoryx-Download-Check", "Accept": "application/vnd.github+json" if "api.github.com" in url else "application/octet-stream"})
            with opener.open(req, timeout=25) as response:
                validate_url(response.url)
                if response.status != 200:
                    raise RuntimeError(f"HTTP {response.status}")
                data = response.read(limit + 1)
                if len(data) > limit:
                    raise RuntimeError("Download exceeds the audit size limit")
                return data
        except (error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(2 * (attempt + 1))


def asset_digest(url, limit=512_000_000):
    validate_url(url)
    opener = request.build_opener(PublicRedirects())
    for attempt in range(3):
        try:
            digest, size = hashlib.sha256(), 0
            req = request.Request(url, headers={"User-Agent": "Framoryx-Download-Check", "Accept": "application/octet-stream"})
            with opener.open(req, timeout=30) as response:
                validate_url(response.url)
                if response.status != 200:
                    raise RuntimeError(f"HTTP {response.status}")
                while block := response.read(1024 * 1024):
                    size += len(block)
                    if size > limit:
                        raise RuntimeError("Asset exceeds the audit size limit")
                    digest.update(block)
            return size, digest.hexdigest()
        except (error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(2 * (attempt + 1))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", default="NeTriXoN-BoT/Framoryx")
    parser.add_argument("--all-assets", action="store_true", help="Require and verify both desktop platforms and clean source.")
    args = parser.parse_args()
    owner, name = args.repository.split("/")
    if any(not part or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-" for c in part) for part in (owner, name)):
        raise RuntimeError("Invalid repository name")
    base = f"https://github.com/{owner}/{name}"
    api = f"https://api.github.com/repos/{owner}/{name}"
    repository = json.loads(download(api, 2_000_000))
    if repository["private"]:
        raise RuntimeError("Downloads require a public repository")
    release = json.loads(download(api + "/releases/latest", 2_000_000))
    if release["draft"] or release["prerelease"]:
        raise RuntimeError("Latest must be a published stable release")
    assets = {item["name"]: item for item in release["assets"]}
    expected_assets = ALL_ASSETS if args.all_assets else (ASSET,)
    for required in [name + suffix for name in expected_assets for suffix in ("", ".sha256")]:
        if required not in assets:
            raise RuntimeError("Latest release is missing " + required)
    readme = (Path(__file__).resolve().parents[1] / "README.md").read_text(encoding="utf-8")
    routes = {canonical_route(url) for url in re.findall(r'href="([^"]+)"', readme)}
    windows = base + "/releases/latest/download/" + ASSET
    latest = base + "/releases/latest"
    if canonical_route(windows) not in routes or canonical_route(latest) not in routes:
        raise RuntimeError("README buttons must use the stable latest-release routes")
    download(latest, 5_000_000)
    verified = []
    for name in expected_assets:
        url = base + "/releases/latest/download/" + name
        if args.all_assets and name.endswith(".AppImage") and canonical_route(url) not in routes:
            raise RuntimeError("README Linux button must use the latest AppImage route")
        checksum = download(url + ".sha256", 4096).decode("utf-8").split()[0].lower()
        size, digest = asset_digest(url)
        if digest != checksum:
            raise RuntimeError("Public asset does not match its checksum: " + name)
        advertised = assets[name].get("digest")
        if advertised and advertised != "sha256:" + digest:
            raise RuntimeError("Public asset differs from GitHub's asset digest: " + name)
        verified.append({"asset": name, "status": "HTTP 200", "bytes": size, "sha256": digest, "checksum": "matched"})
    print(json.dumps({"anonymous": True, "release": release["tag_name"], "latestPage": "HTTP 200", "assets": verified}, indent=2))


if __name__ == "__main__":
    main()
