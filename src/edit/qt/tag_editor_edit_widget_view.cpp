// SPDX-License-Identifier: GPL-3.0-only

#include "tag_editor_edit_widget_view.hpp"

namespace Invader::EditQt {
    TagEditorEditWidgetView::TagEditorEditWidgetView(QWidget *parent, const std::vector<Parser::ParserStructValue> &values, TagEditorWindow *editor_window, bool primary) : QFrame(parent), values(values), editor_window(editor_window) {
        auto *vbox_layout = new QVBoxLayout();
        this->setLayout(vbox_layout);

        for(auto &value : this->values) {
            auto *widget = TagEditorWidget::generate_widget(this, &value, editor_window);
            if(widget) {
                widgets_to_remove.emplace_back(widget);
                vbox_layout->addWidget(widget);
            }
        }

        // Add a spacer so it doesn't try to evenly space everything if we're too big
        if(primary) {
            auto *spacer = new QSpacerItem(0 ,0);
            vbox_layout->addSpacerItem(spacer);
        }
        else {
            this->setFrameStyle(QFrame::Panel | QFrame::Raised);
            this->setLineWidth(2);
        }
    }
}
