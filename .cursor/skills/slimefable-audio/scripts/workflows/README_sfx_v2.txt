# SFX workflow note

Desktop template picker may show ``audio_stable_audio_3_sfx_v2``.

At skill snapshot time the official JSON was not in the local workflow_templates
cache, so ``audio_stable_audio_3_sfx_v2.json`` here is a Medium UI snapshot for
reference only.

**Runtime:** ``generate_audio.py`` submits ``api_stable_audio_3_sfx_v2.json``,
which loads checkpoint ``stable_audio_3_small_sfx.safetensors`` (present on this
machine) with short duration. If that checkpoint is missing, install Stable Audio
Small-SFX or edit the API JSON to use ``stable_audio_3_medium.safetensors``.
