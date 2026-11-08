"""Apply reviewed edits to the checksum-pinned runtime dependencies."""
from pathlib import Path
import sys

source=Path(sys.argv[1])
def replace(relative,old,new,count=1):
    path=source/relative
    text=path.read_text(encoding='utf-8')
    if text.count(old)==count:
        path.write_text(text.replace(old,new),encoding='utf-8')
    elif text.count(new)!=count:
        raise RuntimeError('Unexpected pinned dependency context: '+relative)

replace('subprojects/imgui-1.91.9b/imgui_demo.cpp',
        'sprintf(overlay, "avg %f", average);','snprintf(overlay, sizeof(overlay), "avg %f", average);')
replace('subprojects/imgui-1.91.9b/imgui_demo.cpp',
        'sprintf(name, "(%.1f,%.1f)", alignment.x, alignment.y);','snprintf(name, sizeof(name), "(%.1f,%.1f)", alignment.x, alignment.y);')
replace('subprojects/spdlog-1.14.1/include/spdlog/fmt/bundled/format-inl.h',
        '(z_mul.is_integer & !include_right_endpoint)','(z_mul.is_integer && !include_right_endpoint)')
replace('subprojects/implot-0.16/implot_items.cpp',
        '#include "implot_internal.h"','#include "implot_internal.h"\n#include "../../scalar_formats.hpp"')
replace('subprojects/implot-0.16/implot_items.cpp',
        'ImFormatString(buffer, 32, fmt, (double)values[i]);',
        'ImFormatString(buffer, 32, fg::scalarFormatLiteral(fmt, fg::ScalarFormat::Double), (double)values[i]);')
replace('subprojects/implot-0.16/implot_items.cpp',
        'ImFormatString(buff, 32, fmt, values[i]);',
        'ImFormatString(buff, 32, fg::scalarFormatLiteral(fmt, fg::ScalarFormat::Double), (double)values[i]);',2)
print('Applied bounded formatting and numeric predicate hardening to pinned dependencies.')
