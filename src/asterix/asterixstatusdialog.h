/*
 * This file is part of OpenATS COMPASS.
 *
 * COMPASS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * COMPASS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with COMPASS. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ASTERIXSTATUSDIALOG_H
#define ASTERIXSTATUSDIALOG_H

#include <QDialog>

#include "boost/date_time/posix_time/posix_time.hpp"

class QLabel;
class QPushButton;
class QGridLayout;

class ASTERIXStatusDialog : public QDialog
{
    Q_OBJECT

  signals:
    void closeSignal();

  public slots:
    void okClickedSlot();

  public:
    explicit ASTERIXStatusDialog(const std::string& filename, bool test, bool mapping_stubs,
                                 QWidget* parent = nullptr, Qt::WindowFlags f = 0);

    void markStartTime();
    void setDone();

    void numFrames(unsigned int cnt);
    void numRecords(unsigned int cnt);
    void numErrors(unsigned int cnt);
    void addNumMapped(unsigned int cnt);
    void addNumNotMapped(unsigned int cnt);
    void addNumCreated(unsigned int cnt);
    void addNumInserted(const std::string& dbo_name, unsigned int cnt);

    void setCategoryCounts(const std::map<unsigned int, size_t>& counts);
    void addMappedCounts(const std::map<unsigned int, std::pair<size_t, size_t>>& counts);

    size_t numFrames() const;
    size_t numRecords() const;
    size_t numErrors() const;
    std::map<std::string, size_t> dboInsertedCounts() const;
    std::string elapsedTimeStr() const;

    std::string recordsInsertedRateStr() const;
    size_t numRecordsInserted() const;

  private:
    std::string filename_;
    bool test_{false};
    bool mapping_stubs_{false};

    size_t num_frames_{0};
    size_t num_records_{0};
    size_t num_errors_{0};
    size_t records_mapped_{0};
    size_t records_not_mapped_{0};
    size_t records_created_{0};
    size_t records_inserted_{0};

    std::map<unsigned int, size_t> category_read_counts_;
    std::map<unsigned int, std::pair<size_t, size_t>> category_mapped_counts_;

    boost::posix_time::ptime start_time_;
    boost::posix_time::ptime stop_time_;
    boost::posix_time::time_duration time_diff_;
    std::string elapsed_time_str_;
    std::string last_log_elapsed_time_str_;

    QLabel* time_label_{nullptr};

    QLabel* num_frames_label_{nullptr};
    QLabel* num_records_label_{nullptr};
    QLabel* num_errors_label_{nullptr};
    QLabel* num_records_rate_label_{nullptr};
    QLabel* records_mapped_label_{nullptr};
    QLabel* records_not_mapped_label_{nullptr};
    QLabel* records_created_label_{nullptr};
    QLabel* records_inserted_label_{nullptr};
    QLabel* records_inserted_rate_label_{nullptr};

    std::string records_inserted_rate_str_;

    std::map<std::string, size_t> dbo_inserted_counts_;

    QGridLayout* cat_counters_grid_{nullptr};
    QGridLayout* dbo_counters_grid_{nullptr};
    QPushButton* ok_button_{nullptr};

    void updateCategoryGrid();
    void updateDBObjectGrid();
    void updateTime();
};

#endif  // ASTERIXSTATUSDIALOG_H
