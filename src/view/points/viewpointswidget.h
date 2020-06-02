#ifndef VIEWPOINTSWIDGET_H
#define VIEWPOINTSWIDGET_H

#include <QWidget>

class ViewManager;
class ViewPointsToolWidget;
class ViewPointsTableModel;

class QTableView;
class QPushButton;
class QSortFilterProxyModel;

class ViewPointsWidget : public QWidget
{
    Q_OBJECT

  public slots:
    void exportSlot();
    void exportPDFSlot();
    void deleteAllSlot();
    void importSlot();

    void selectPreviousSlot();
    void selectNextSlot();

    void setSelectedOpenSlot();
    void setSelectedClosedSlot();
    void setSelectedTodoSlot();
    void editCommentSlot();

    void currentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    //void onTableClickedSlot(const QModelIndex& current);

    void loadingStartedSlot();
    void allLoadingDoneSlot();

    void typesChangedSlot(QStringList types);

public:
    ViewPointsWidget(ViewManager& view_manager);
    virtual ~ViewPointsWidget();

    void resizeColumnsToContents();

    //    void selectNextOpen();
//    void openCurrentSelectNext();
//    void closeCurrentSelectNext();

    ViewPointsTableModel* tableModel() const;

    QStringList types() const;
    QStringList filteredTypes() const;

    void filterType (QString type);
    void showAllTypes ();
    void showNoTypes ();

    void updateFilteredTypes ();

    QStringList columns() const;
    QStringList filteredColumns() const;

    void filterColumn (QString name);
    void showOnlyMainColumns ();
    void showAllColumns ();
    void showNoColumns ();

    void updateFilteredColumns ();

private:
    ViewManager& view_manager_;

    ViewPointsToolWidget* tool_widget_{nullptr};

    QTableView* table_view_{nullptr};
    QSortFilterProxyModel* proxy_model_{nullptr};
    ViewPointsTableModel* table_model_{nullptr};

    QPushButton* import_button_{nullptr};
    QPushButton* delete_all_button_{nullptr};
    QPushButton* export_button_{nullptr};
    QPushButton* export_pdf_button_{nullptr};

    QStringList types_;
    QStringList filtered_types_;

    QStringList columns_;
    QStringList filtered_columns_;

    bool load_in_progress_ {false};
    bool restore_focus_ {false};
};

#endif // VIEWPOINTSWIDGET_H
