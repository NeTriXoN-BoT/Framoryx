"""Verify the public latest-release buttons without credentials or browser cookies."""
import argparse
import hashlib
import json
from pathlib import Path
import time
from urllib import error, parse, request

ALLOWED_HOSTS = {"github.com", "api.github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com"}
ASSET = "Framoryx-Windows-x64.zip"


class PublicRedirects(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        validate_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def validate_url(url):
    parsed = parse.urlsplit(url)
    if parsed.scheme != "https" or parsed.hostname not in ALLOWED_HOSTS or parsed.username or parsed.password:
        raise RuntimeError("Unexpected download destination")


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", default="NeTriXoN-BoT/Framoryx")
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
    for required in (ASSET, ASSET + ".sha256"):
        if required not in assets:
            raise RuntimeError("Latest release is missing " + required)
    readme = (Path(__file__).resolve().parents[1] / "README.md").read_text(encoding="utf-8")
    windows = base + "/releases/latest/download/" + ASSET
    latest = base + "/releases/latest"
    if f'href="{windows}"' not in readme or f'href="{latest}"' not in readme:
        raise RuntimeError("README buttons must use the stable latest-release routes")
    download(latest, 5_000_000)
    checksum = download(windows + ".sha256", 4096).decode("utf-8").split()[0].lower()
    data = download(windows, 128_000_000)
    digest = hashlib.sha256(data).hexdigest()
    if digest != checksum:
        raise RuntimeError("Public ZIP does not match its public checksum")
    advertised = assets[ASSET].get("digest")
    if advertised and advertised != "sha256:" + digest:
        raise RuntimeError("Public ZIP differs from GitHub's asset digest")
    print(json.dumps({"anonymous": True, "release": release["tag_name"], "latestPage": "HTTP 200", "windowsZip": "HTTP 200", "checksum": "matched", "bytes": len(data), "sha256": digest}, indent=2))


if __name__ == "__main__":
    main()
