/* 表示3択の区画とヘルプ「?」との群間（96 DPI の論理 px）。窓幅から folio_window が 1 か所で決め、
 * 描画・当たり判定・UIA・tooltip が同じ値を使う（ADR0060 の決定 10・#349）。 */
#ifndef NENEFOLIO_LAYOUT_METRICS_H
#define NENEFOLIO_LAYOUT_METRICS_H

struct layout_metrics
{
    int width;
    int gap;
    bool chooser; /* DPI済み残量から導く表示。保存しない。 */
};

#endif
