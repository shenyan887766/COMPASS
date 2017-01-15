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

/*
 * DBCondition.h
 *
 *  Created on: Nov 6, 2011
 *      Author: sk
 */

#ifndef DBFILTERCONDITION_H_
#define DBFILTERCONDITION_H_

#include <QObject>
#include "Configurable.h"
#include "DBOVariable.h"
#include "DBOVariableMinMaxObserver.h"

class QWidget;
class QLineEdit;
class QLabel;

class DBFilter;

/**
 * @brief Filtering condition for SQL-clauses
 *
 * @detail Uses a Qt-event system, hold a widget with control elements. Uses the events to receive change-messages from the widget,
 * and signals possible changes to the parent DBFilter.
 *
 * A condition consists of a DBOVariable, and operator and a value e.g. 'TOD <= 0.1'. A number of operators are supported, the value
 * also has a specific reset value (e.g. original value, minimum of variable, maximum of variable.
 */
class DBFilterCondition : public QObject, public Configurable, public DBOVariableMinMaxObserver
{
    Q_OBJECT

private slots:
    /// @brief Slot to be called when the value of the condition has been changed
    void valueChanged ();

signals:
    /// @brief Signal which is emitted when a change in the condition has occurred
    void possibleFilterChange();

public:
    /// @brief Constructor
    DBFilterCondition(std::string instance_id, DBFilter *filter_parent);
    /// @brief Desctructor
    virtual ~DBFilterCondition();

    /// @brief Invert the condition. Not used yet.
    void invert ();
    /// @brief Returns if condition is active for the DBO type
    bool filters (const std::string &dbo_type);
    /// @brief Returns condition string for a DBO type
    std::string getConditionString (const std::string &dbo_type, bool &first, std::vector<std::string> &variable_names);

    /// @brief Returns the widget
    QWidget *getWidget () { assert(widget_); return widget_;};

    /// @brief Updates the GUI elements
    void update ();

    /// @brief Returns changed flag
    bool getChanged () { return changed_; };
    /// @brief Sets changed flag
    void setChanged (bool changed) { changed_=changed; };

    /// @brief Returns DBOVariable which is used in the condition
    DBOVariable *getVariable () { return variable_; }
    /// @brief Sets the DBOVariable which is used in the condition
    void setVariable (DBOVariable *variable);

    /// @brief Returns if absolute value of the DBOVariable should be used
    bool getAbsoluteValue () { return absolute_value_; }
    /// @brief Sets if absolute value of the DBOVariable should be used
    void setAbsoluteValue (bool abs) { absolute_value_=abs; }

    /// @brief Returns operator
    std::string getOperator () { return operator_; }
    /// @brief Sets operator
    void setOperator (std::string operator_val) { operator_ =operator_val;}

    /// @brief Returns the current value
    std::string getValue () { return value_; }
    /// @brief Sets the current value
    void setValue (std::string value) { value_ =value;}

    /// @brief Returns the reset value
    std::string getResetValue () { return reset_value_; }
    /// @brief Sets the reset value
    void setResetValue (std::string reset_value) { reset_value_ =reset_value;}

    /// @brief Resets condition by setting value_ to reset_value_
    void reset ();

    virtual void notifyMinMax (DBOVariable *variable);

private:
    /// @brief Parent filter
    DBFilter *filter_parent_;
    /// @brief Operator
    std::string operator_;
    /// @brief AND operator flag, not used yet.
    bool op_and_;
    /// @brief Absolute value flag.
    bool absolute_value_;
    /// @brief Current value
    std::string value_;
    /// @brief Reset value
    std::string reset_value_;
    /// @brief DBO type of variable
    std::string variable_dbo_type_;
    /// @brief DBO variable identifier
    std::string variable_name_;
    /// @brief Pointer to DBO variable
    DBOVariable *variable_;
    /// @brief Changed flag
    bool changed_;

    /// @brief Widget with condition elements
    QWidget *widget_;
    /// @brief Value edit field
    QLineEdit *edit_;
    /// @brief Variable name and operator label
    QLabel *label_;
};

#endif /* DBFILTERCONDITION_H_ */