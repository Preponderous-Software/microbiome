# Link-preview card

`assets/og.png` is the picture a shared link to this site unfurls with (`og:image`). It is drawn from [`card.json`](card.json) by [community-site-kit's `tools/og-card`](https://github.com/kingdom-community/community-site-kit/tree/c5fdb46f139db840a74f93f2d955bfc9fb5e1c50/tools/og-card): a small 3D scene rendered with [Strata](https://github.com/Preponderous-Software/strata) 0.6.0, with the title over it. The scene is built from the seed alone and contains no site data. [`card.provenance.json`](card.provenance.json) records what made the committed PNG: the tool commit, the Strata library's SHA-256, the Pillow and font hashes, and the PNG's own SHA-256.

The PNG is pre-rendered and committed, so nothing 3D runs on the page.

## Changing the card

Edit `card.json`. If the picture or its text changes, rewrite `alt` to match. Copy the new `alt` into both `og:image:alt` and `twitter:image:alt` in `src/webServer.cpp`; `./mb_web_tests` fails until they match. After redrawing, run `make og-image-header` and commit `src/header/ogImage.h` too. The card is a palette PNG (`colors`: 256) because it is compiled into the server. Then, on Linux x86-64:

```sh
git clone https://github.com/kingdom-community/community-site-kit /tmp/csk
git -C /tmp/csk checkout c5fdb46f139db840a74f93f2d955bfc9fb5e1c50
python3 -m venv /tmp/og-venv
/tmp/og-venv/bin/pip install -r /tmp/csk/tools/og-card/requirements.txt
sudo apt-get install libegl1 libgl1-mesa-dri fonts-dejavu-core
/tmp/og-venv/bin/python /tmp/csk/tools/og-card/og_card.py og-card/card.json
```

Commit the PNG and both JSON files. Scrapers cache a card by its URL for days, so a materially new picture is best given a new file name.
