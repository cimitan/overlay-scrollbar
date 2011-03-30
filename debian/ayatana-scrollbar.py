def add_info(report):
    if report.has_key("ProcMaps") and "liboverlay-scrollbar" in report["ProcMaps"]:
        report['Tags'] = report.get('Tags', '') + ' ayatana-scrollbar'
    if report.has_key("Stacktrace") and "os-scrollbar.c" in report["Stacktrace"]:
        report['Tags'] = report.get('Tags', '') + ' ayatana-scrollbar_scrollbar'
    if report.has_key("Stacktrace") and "os-thumb.c" in report["Stacktrace"]:
        report['Tags'] = report.get('Tags', '') + ' ayatana-scrollbar_thumb'
    if report.has_key("Stacktrace") and "os-pager.c" in report["Stacktrace"]:
        report['Tags'] = report.get('Tags', '') + ' ayatana-scrollbar_pager'
