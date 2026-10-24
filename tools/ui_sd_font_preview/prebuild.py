Import('env')
import subprocess

subprocess.run([env.subst('$PYTHONEXE'),
                env.subst('$PROJECT_DIR') + '/tools/ui_sd_font_preview/prepare_codepoints.py',
                '--check'], check=True)
