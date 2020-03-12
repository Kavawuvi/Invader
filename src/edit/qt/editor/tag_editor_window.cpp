// SPDX-License-Identifier: GPL-3.0-only

#include <QCloseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include <QScrollArea>
#include <QLabel>
#include <QApplication>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <filesystem>
#include <invader/file/file.hpp>
#include <invader/tag/parser/parser.hpp>
#include "../tree/tag_tree_window.hpp"
#include "widget/tag_editor_widget.hpp"
#include "widget/tag_editor_edit_widget_view.hpp"
#include "../tree/tag_tree_dialog.hpp"
#include "subwindow/tag_editor_subwindow.hpp"
#include "subwindow/tag_editor_bitmap_subwindow.hpp"

namespace Invader::EditQt {
    TagEditorWindow::TagEditorWindow(QWidget *parent, TagTreeWindow *parent_window, const File::TagFile &tag_file) : QMainWindow(parent), parent_window(parent_window), file(tag_file) {
        // If we're loading an existing tag, open it and parse it
        if(tag_file.tag_path.size() != 0) {
            this->make_dirty(false);
            auto open_file = File::open_file(tag_file.full_path.string().c_str());
            if(!open_file.has_value()) {
                char formatted_error[1024];
                std::snprintf(formatted_error, sizeof(formatted_error), "Failed to open %s. Make sure it exists and you have permission to open it.", tag_file.full_path.string().c_str());
                QMessageBox(QMessageBox::Icon::Critical, "Error", formatted_error, QMessageBox::Ok, this).exec();
                this->close();
                return;
            }
            try {
                this->parser_data = Parser::ParserStruct::parse_hek_tag_file(open_file->data(), open_file->size(), false).release();
            }
            catch(std::exception &e) {
                char formatted_error[1024];
                std::snprintf(formatted_error, sizeof(formatted_error), "Failed to open %s due to an exception error:\n\n%s", tag_file.full_path.string().c_str(), e.what());
                QMessageBox(QMessageBox::Icon::Critical, "Error", formatted_error, QMessageBox::Ok, this).exec();
                this->close();
                return;
            }

            if(this->parser_data->check_for_broken_enums(false)) {
                char formatted_error[1024];
                std::snprintf(formatted_error, sizeof(formatted_error), "Failed to parse %s due to enumerators being out-of-bounds.\n\nThe tag appears to be corrupt.", tag_file.full_path.string().c_str());
                QMessageBox(QMessageBox::Icon::Critical, "Error", formatted_error, QMessageBox::Ok, this).exec();
                this->close();
                return;
            }
        }
        else {
            this->make_dirty(true);
            this->parser_data = Parser::ParserStruct::generate_base_struct(tag_file.tag_class_int).release();
            if(!this->parser_data) {
                char formatted_error[1024];
                std::snprintf(formatted_error, sizeof(formatted_error), "Failed to create a %s.", tag_class_to_extension(tag_file.tag_class_int));
                QMessageBox(QMessageBox::Icon::Critical, "Error", formatted_error, QMessageBox::Ok, this).exec();
                this->close();
                return;
            }
        }

        // Make and set our menu bar
        QMenuBar *bar = new QMenuBar(this);
        this->setMenuBar(bar);

        // File menu
        auto *file_menu = bar->addMenu("File");

        auto *save = file_menu->addAction("Save");
        save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
        save->setShortcut(QKeySequence::Save);
        connect(save, &QAction::triggered, this, &TagEditorWindow::perform_save);

        auto *save_as = file_menu->addAction("Save as...");
        save_as->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
        save_as->setShortcut(QKeySequence::SaveAs);
        connect(save_as, &QAction::triggered, this, &TagEditorWindow::perform_save_as);

        file_menu->addSeparator();

        auto *close = file_menu->addAction("Close");
        close->setShortcut(QKeySequence::Close);
        close->setIcon(QIcon::fromTheme(QStringLiteral("document-close")));
        connect(close, &QAction::triggered, this, &TagEditorWindow::close);

        // Add another widget to our view?
        QFrame *extra_widget_panel = nullptr;
        QPushButton *extra_widget;
        switch(tag_file.tag_class_int) {
            case TagClassInt::TAG_CLASS_BITMAP:
            case TagClassInt::TAG_CLASS_EXTENDED_BITMAP:
                extra_widget = new QPushButton("Preview bitmap");
                break;
            // case TagClassInt::TAG_CLASS_SOUND:
            //     extra_widget = new QPushButton("Preview sound");
            //     break;
            // case TagClassInt::TAG_CLASS_GBXMODEL:
            // case TagClassInt::TAG_CLASS_SCENARIO_STRUCTURE_BSP:
            //     extra_widget = new QPushButton("Preview model");
            //     break;
            // case TagClassInt::TAG_CLASS_NEW_UNIT_HUD_INTERFACE:
            // case TagClassInt::TAG_CLASS_NEW_WEAPON_HUD_INTERFACE:
            // case TagClassInt::TAG_CLASS_NEW_UI_WIDGET_DEFINITION:
            // case TagClassInt::TAG_CLASS_UNIT_HUD_INTERFACE:
            // case TagClassInt::TAG_CLASS_WEAPON_HUD_INTERFACE:
            //     extra_widget = new QPushButton("Preview interface");
            //     break;
            // case TagClassInt::TAG_CLASS_SHADER:
            // case TagClassInt::TAG_CLASS_SHADER_MODEL:
            // case TagClassInt::TAG_CLASS_SHADER_ENVIRONMENT:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_CHICAGO:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_CHICAGO_EXTENDED:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_GENERIC:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_GLASS:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_GLSL:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_METER:
            // case TagClassInt::TAG_CLASS_SHADER_TRANSPARENT_WATER:
            //     extra_widget = new QPushButton("Preview shader");
            //     break;
            // case TagClassInt::TAG_CLASS_SCENARIO:
            //     extra_widget = new QPushButton("Edit scenario");
            //     break;
            default:
                extra_widget = nullptr;
                break;
        }
        if(extra_widget) {
            extra_widget_panel = new QFrame();
            QHBoxLayout *extra_layout = new QHBoxLayout();
            extra_widget_panel->setLayout(extra_layout);
            extra_layout->addWidget(extra_widget);
            extra_layout->setMargin(4);
            connect(extra_widget, &QPushButton::clicked, this, &TagEditorWindow::show_subwindow);
        }

        // Set up the scroll area and widgets
        auto *scroll_view = new QScrollArea();
        scroll_view->setWidgetResizable(true);
        auto values = std::vector<Parser::ParserStructValue>(this->parser_data->get_values());
        this->setCentralWidget(scroll_view);
        scroll_view->setWidget(new TagEditorEditWidgetView(nullptr, values, this, true, extra_widget_panel));

        // View menu
        auto *view_menu = bar->addMenu("View");
        auto *toggle_fullscreen = view_menu->addAction("Toggle Full Screen");
        toggle_fullscreen->setShortcut(QKeySequence::FullScreen);
        toggle_fullscreen->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
        connect(toggle_fullscreen, &QAction::triggered, this, &TagEditorWindow::toggle_fullscreen);

        // Lock the scroll view and window to a set width
        int max_width = scroll_view->widget()->width() + qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 16;

        // Center this
        this->setGeometry(
            QStyle::alignedRect(
                Qt::LeftToRight,
                Qt::AlignCenter,
                QSize(max_width, 600),
                QGuiApplication::primaryScreen()->geometry()
            )
        );

        // We did it!
        this->successfully_opened = true;
    }

    void TagEditorWindow::closeEvent(QCloseEvent *event) {
        bool accept;
        if(dirty) {
            char message_entire_text[512];
            if(this->file.tag_path.size() == 0) {
                std::snprintf(message_entire_text, sizeof(message_entire_text), "This is a new %s file.\nDo you want to save your changes?", HEK::tag_class_to_extension(this->file.tag_class_int));
            }
            else {
                std::snprintf(message_entire_text, sizeof(message_entire_text), "This file \"%s\" has been modified.\nDo you want to save your changes?", this->file.full_path.string().c_str());
            }
            QMessageBox are_you_sure(QMessageBox::Icon::Question, "Unsaved changes", message_entire_text, QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
            switch(are_you_sure.exec()) {
                case QMessageBox::Save:
                    accept = this->perform_save();
                    break;
                case QMessageBox::Cancel:
                    accept = false;
                    break;
                case QMessageBox::Discard:
                    accept = true;
                    break;
                default:
                    std::terminate();
            }
        }
        else {
            accept = true;
        }

        // Delete subwindow
        if(accept && this->subwindow) {
            this->subwindow->deleteLater();
            this->subwindow = nullptr;
        }

        event->setAccepted(accept);

        // Clean up
        this->parent_window->cleanup_windows(this);
    }

    bool TagEditorWindow::perform_save() {
        if(this->file.tag_path.size() == 0) {
            printf("CHU!\n");
            return this->perform_save_as();
        }

        // Save; benchmark
        auto start = std::chrono::steady_clock::now();
        auto tag_data = parser_data->generate_hek_tag_data(this->file.tag_class_int);
        auto result = Invader::File::save_file(this->file.full_path.string().c_str(), tag_data);
        if(!result) {
            std::fprintf(stderr, "perform_save() failed\n");
        }
        else {
            this->make_dirty(false);
            auto end = std::chrono::steady_clock::now();
            std::printf("Saved %s in %zu ms\n", this->get_file().full_path.string().c_str(), std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        }
        return result;
    }

    bool TagEditorWindow::perform_save_as() {
        TagTreeDialog d(this, this->parent_window, this->file.tag_class_int);
        if(d.exec() == QMessageBox::Accepted) {
            this->file = *d.get_tag();
            std::filesystem::create_directories(this->file.full_path.parent_path());
            auto result = this->perform_save();
            this->parent_window->reload_tags();
            return result;
        }

        return false;
    }

    void TagEditorWindow::make_dirty(bool dirty) {
        this->dirty = dirty;

        char title_bar[512];
        if(this->file.tag_path.size() == 0) {
            std::snprintf(title_bar, sizeof(title_bar), "Untitled %s", HEK::tag_class_to_extension(this->file.tag_class_int));
        }
        else {
            const char *asterisk = dirty ? " *" : "";
            std::snprintf(title_bar, sizeof(title_bar), "%s%s", this->file.tag_path.c_str(), asterisk);
        }
        this->setWindowTitle(title_bar);

        if(this->subwindow) {
            if(this->subwindow->isHidden()) {
                this->subwindow->deleteLater();
                this->subwindow = nullptr;
            }
            else {
                this->subwindow->setWindowTitle(this->file.tag_path.c_str());
                this->subwindow->update();
            }
        }
    }

    const File::TagFile &TagEditorWindow::get_file() const noexcept {
        return this->file;
    }

    void TagEditorWindow::toggle_fullscreen() {
        if(this->isFullScreen()) {
            this->showNormal();
        }
        else {
            this->showFullScreen();
        }
    }

    TagEditorWindow::~TagEditorWindow() {
        if(this->subwindow) {
            this->subwindow->deleteLater();
            this->subwindow = nullptr;
        }
        delete this->parser_data;
    }

    void TagEditorWindow::show_subwindow() {
        if(!this->subwindow) {
            switch(this->file.tag_class_int) {
                case TagClassInt::TAG_CLASS_BITMAP:
                case TagClassInt::TAG_CLASS_EXTENDED_BITMAP:
                    this->subwindow = new TagEditorBitmapSubwindow(this);
                    break;
                default:
                    std::terminate();
            }
        }
        this->subwindow->show();
        this->subwindow->setWindowState(Qt::WindowState::WindowActive);
    }
}