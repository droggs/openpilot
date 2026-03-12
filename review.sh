#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

rm -f REVIEW.md
codex exec -m "gpt-5.4" --dangerously-bypass-approvals-and-sandbox "review my PR for imgui cabana. we're trying to rewrite cabana in imgui (from Qt) and this is the final review before we ship it to production. we need 1:1 functionality and UX match, absnolutely no features can be missing. stability and code beauty are also very important. thoroughly review all code for the imgui cabana to assess what's incomplete, incorrect, or buggy and present a nice list. write down your review in REVIEW.md. if it's 100% all good, just say it's all good and we're done. but remember we're not done until 1:1 features and UX match!"
