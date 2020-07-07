/*
 * This file is part of ATSDB.
 *
 * ATSDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ATSDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with ATSDB.  If not, see <http://www.gnu.org/licenses/>.
 */

//#include <iostream>

#include "allbuffertablewidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QTimer>

#include "allbuffertablemodel.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "buffer.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbovariable.h"
#include "dbovariableset.h"
#include "listboxviewdatasource.h"
#include "logger.h"
#include "viewselection.h"

// using namespace Utils;

AllBufferTableWidget::AllBufferTableWidget(ListBoxView& view, ListBoxViewDataSource& data_source,
                                           QWidget* parent, Qt::WindowFlags f)
    : QWidget(parent, f), view_(view), data_source_(data_source)
{
    //setAutoFillBackground(true);

    QVBoxLayout* layout = new QVBoxLayout();

    table_ = new QTableView(this);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setSelectionMode(QAbstractItemView::ContiguousSelection);
    model_ = new AllBufferTableModel(this, data_source_);
    table_->setModel(model_);

    connect(model_, SIGNAL(exportDoneSignal(bool)), this, SLOT(exportDoneSlot(bool)));

    layout->addWidget(table_);
    table_->show();

    setLayout(layout);
}

AllBufferTableWidget::~AllBufferTableWidget() {}

void AllBufferTableWidget::clear()
{
    assert(model_);

    model_->clearData();
}

void AllBufferTableWidget::show(
        std::shared_ptr<Buffer> buffer)  //, DBOVariableSet *variables, bool database_view
{
    assert(buffer);

    logdbg << "AllBufferTableWidget: show: buffer size " << buffer->size() << " properties "
           << buffer->properties().size();
    assert(table_);
    assert(model_);

    model_->setData(buffer);
    table_->resizeColumnsToContents();

    logdbg << " AllBufferTableWidget: show: end";
}

void AllBufferTableWidget::exportSlot(bool overwrite)
{
    loginf << "AllBufferTableWidget: exportSlot";

    QFileDialog dialog(nullptr);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter("CSV Files (*.csv)");
    dialog.setDefaultSuffix("csv");
    dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);

    if (!overwrite)
        dialog.setOption(QFileDialog::DontConfirmOverwrite);

    QStringList file_names;
    if (dialog.exec())
        file_names = dialog.selectedFiles();

    QString filename;

    if (file_names.size() == 1)
        filename = file_names.at(0);

    if (filename.size())
    {
        if (!filename.endsWith(".csv"))  // in case of qt bug
            filename += ".csv";

        loginf << "AllBufferTableWidget: exportSlot: export filename " << filename.toStdString();
        assert(model_);
        model_->saveAsCSV(filename.toStdString(), overwrite);
    }
    else
    {
        emit exportDoneSignal(true);
    }
}

void AllBufferTableWidget::exportDoneSlot(bool cancelled) { emit exportDoneSignal(cancelled); }

void AllBufferTableWidget::showOnlySelectedSlot(bool value)
{
    loginf << "AllBufferTableWidget: showOnlySelectedSlot: " << value;

    assert(model_);
    model_->showOnlySelected(value);
    assert(table_);
    table_->resizeColumnsToContents();
}

void AllBufferTableWidget::usePresentationSlot(bool use_presentation)
{
    assert(model_);
    model_->usePresentation(use_presentation);
    assert(table_);
    table_->resizeColumnsToContents();
}

void AllBufferTableWidget::showAssociationsSlot(bool value)
{
    assert(model_);
    model_->showAssociations(value);
    assert(table_);
    table_->resizeColumnsToContents();
}

void AllBufferTableWidget::resetModel()
{
    assert(model_);
    model_->reset();
}

void AllBufferTableWidget::updateToSelection()
{
    assert(model_);
    model_->updateToSelection();
    assert(table_);
    table_->resizeColumnsToContents();
}

void AllBufferTableWidget::resizeColumns()
{
    assert(table_);
    table_->resizeColumnsToContents();
}

void AllBufferTableWidget::selectSelectedRows()
{
    loginf << "AllBufferTableWidget: selectSelectedRows";

    assert(table_);
    assert(model_);
    std::pair<int,int> rows = model_->getSelectedRows();

    if (rows.first >= 0 && rows.second >= 0)
    {
        loginf << "AllBufferTableWidget: selectSelectedRows: rows " << rows.first << " to " << rows.second;

        assert (rows.first <= rows.second);

        QModelIndex first = model_->index(rows.first, 0, QModelIndex());
        assert (first.isValid());

        QModelIndex last = model_->index(rows.second, 0, QModelIndex());
        assert (last.isValid());

        table_->selectionModel()->select(QItemSelection(first, last),
                                         QItemSelectionModel::Select | QItemSelectionModel::Rows);

        //table_->scrollTo(first, QAbstractItemView::PositionAtCenter);
        // needed, maybe because model is reset
        QTimer::singleShot(10, [this,first]{table_->scrollTo(first, QAbstractItemView::PositionAtCenter);});
    }
    else
        loginf << "AllBufferTableWidget: selectSelectedRows: nothing selected";
}

ListBoxView& AllBufferTableWidget::view() const { return view_; }

void AllBufferTableWidget::keyPressEvent(QKeyEvent* event)
{
    loginf << "AllBufferTableWidget: keyPressEvent: got keypressed";

    assert(table_);

    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_C)
        {
            loginf << "AllBufferTableWidget: keyPressEvent: copying";

            QAbstractItemModel* model = table_->model();
            QItemSelectionModel* selection = table_->selectionModel();
            QModelIndexList indexes = selection->selectedIndexes();

            QString selected_text;
            QString selected_headers;
            // You need a pair of indexes to find the row changes
            QModelIndex previous = indexes.first();
            unsigned int row_count = 0;

            selected_headers = model->headerData(previous.column(), Qt::Horizontal).toString();
            selected_text = model->data(previous).toString();
            indexes.removeFirst();

            foreach (const QModelIndex& current, indexes)
            {
                // If you are at the start of the row the row number of the previous index
                // isn't the same.  Text is followed by a row separator, which is a newline.
                if (current.row() != previous.row())
                {
                    selected_text.append('\n');

                    if (!row_count)  // first row
                        selected_headers.append('\n');

                    ++row_count;

                    if (row_count == 999)
                    {
                        QMessageBox m_warning(
                                    QMessageBox::Warning, "Too Many Rows Selected",
                                    "If more than 1000 lines are selected, only the first 1000 are copied.",
                                    QMessageBox::Ok);
                        m_warning.exec();
                        break;
                    }
                }
                // Otherwise it's the same row, so append a column separator, which is a tab.
                else
                {
                    if (!row_count)  // first row
                        selected_headers.append(';');

                    selected_text.append(';');
                }

                QVariant data = model->data(current);
                QString text = data.toString();
                // At this point `text` contains the text in one cell
                selected_text.append(text);

                //                loginf << "UGA row " << current.row() << " col " <<
                //                current.column() << " text '"
                //                       << text.toStdString() << "'";

                if (!row_count)  // first row
                    selected_headers.append(
                                model->headerData(current.column(), Qt::Horizontal).toString());

                previous = current;
            }

            QApplication::clipboard()->setText(selected_headers + selected_text);
        }
    }
}

std::vector<std::vector<std::string>> AllBufferTableWidget::getSelectedText ()
{
    std::vector<std::vector<std::string>> data;

    QAbstractItemModel* model = table_->model();
    QItemSelectionModel* selection = table_->selectionModel();
    QModelIndexList indexes = selection->selectedIndexes();

    if (!indexes.size())
        return data;

    QModelIndex previous = indexes.first();

    if (!previous.isValid()) // empty
        return data;

    unsigned int row_count = 0;

    std::vector<std::string> header_data;
    std::vector<std::string> current_row_data;

    header_data.push_back(model->headerData(previous.column(), Qt::Horizontal).toString().toStdString());
    current_row_data.push_back(model->data(previous).toString().toStdString());
    indexes.removeFirst();

    foreach (const QModelIndex& current, indexes)
    {
        // If you are at the start of the row the row number of the previous index
        // isn't the same.  Text is followed by a row separator, which is a newline.
        if (current.row() != previous.row())
        {
            if (!row_count)  // first row
                data.push_back(header_data);

            data.push_back(current_row_data);
            current_row_data.clear();

            ++row_count;

            if (row_count == 999)
            {
                QMessageBox m_warning(
                            QMessageBox::Warning, "Too Many Rows Selected",
                            "If more than 1000 lines are selected, only the first 1000 are copied.",
                            QMessageBox::Ok);
                m_warning.exec();
                break;
            }
        }

        // Otherwise it's the same row, so append a column separator, which is a tab.
        current_row_data.push_back(model->data(current).toString().toStdString());

        if (!row_count)  // first row
            header_data.push_back(model->headerData(current.column(), Qt::Horizontal).toString().toStdString());

        previous = current;
    }

    return data;
}

std::vector<std::vector<std::string>> AllBufferTableWidget::getText (unsigned int max_rows)
{
    std::vector<std::vector<std::string>> data;

    QAbstractItemModel* model = table_->model();

    QModelIndex previous_index = model->index(0, 0);

    if (!previous_index.isValid()) // empty
        return data;

    unsigned int row_count = 0;

    std::vector<std::string> header_data;
    std::vector<std::string> current_row_data;

    header_data.push_back(model->headerData(previous_index.column(), Qt::Horizontal).toString().toStdString());
    current_row_data.push_back(model->data(previous_index).toString().toStdString());

    QModelIndex current_index;

    auto rows = model->rowCount();
    auto cols = model->columnCount();

    bool max_rows_hit = false;

    for (int row_cnt = 0; row_cnt < rows; ++row_cnt)
    {
        for (int col_cnt = 0; col_cnt < cols; ++col_cnt)
        {
            if (row_cnt == 0 && col_cnt == 0) // first, skip for previous
                continue;

            if (row_cnt >= max_rows)
            {
                max_rows_hit = true;
                break;
            }

            current_index = model->index(row_cnt, col_cnt);

            if (!current_index.isValid())
                break;

            // If you are at the start of the row the row number of the previous index
            // isn't the same.  Text is followed by a row separator, which is a newline.
            if (current_index.row() != previous_index.row())
            {
                if (!row_count)  // first row
                    data.push_back(header_data);

                data.push_back(current_row_data);
                current_row_data.clear();

                ++row_count;
            }

            // Otherwise it's the same row, so append a column separator, which is a tab.
            current_row_data.push_back(model->data(current_index).toString().toStdString());

            if (!row_count)  // first row
                header_data.push_back(model->headerData(current_index.column(), Qt::Horizontal).toString().toStdString());

            previous_index = current_index;
        }
    }

    if (max_rows_hit)
    {
        current_row_data.clear();

        for (int col_cnt = 0; col_cnt < cols; ++col_cnt)
            current_row_data.push_back("...");

        data.push_back(current_row_data);
        current_row_data.clear();
    }

    return data;
}
