<?php

/**
 * @file MeetingCloneCreateMeetings
 * 
 * Create multiple cloned nodes. 
 * jrobens@interlated.com.au 201201
 */
class MeetingCloneCreateMeetings {

  const DATE_FORMAT = 'Y-m-d H:i:s';

  protected $start_date;
  protected $interval = 'bi-monthly';
  protected $number = 12;
  protected $template_date_field;
  protected $template_submission_cutoff_field;
  protected $node;
  // It can be 0=Sunday through 6=Saturday (Like 'w' from date()).
  protected $wanted_day_in_week = 5;
  protected $instance_in_month = 2; // second friday. Could be -1 for the last friday.

  /**
   * 
   * @param node $node
   *   meetingclone_start_date = date to start from
   *   meetingclone_count = number of meetings to create
   *   meetingclone_interval = monthly|bi-monthly
   *   meetingclone_wanted_day_in_week = which day in week for recurring meeting (0=Sunday)
   *   meetingclone_instance_in_month = week for recurring meeting in month
   * 
   * @return boolean 
   */

  public function __construct($node) {
    if (!isset($node->meetingclone_start_date)) {
      watchdog('meetingclone', 'Start date not set.');
      return FALSE;
    }

    $this->start_date = $node->meetingclone_start_date;
    $this->number = isset($node->meetingclone_count) ? $node->meetingclone_count : 12;
    $this->interval = isset($node->meetingclone_interval) ? $node->meetingclone_interval : 'bi-monthly';
    $this->wanted_day_in_week = isset($node->meetingclone_weekdaywanted) ? $node->meetingclone_weekdaywanted : 5;
    $this->instance_in_month = isset($node->meetingclone_instanceofmonth) ? $node->meetingclone_instanceofmonth : 2;

    $this->template_date_field = $template_date_field = variable_get('meetingclone_date_field');
    $this->template_submission_cutoff_field = $template_submission_cutoff_field = variable_get('meetingclone_submission_cutoff_field', '');

    if (!isset($this->template_submission_cutoff_field) || $this->template_submission_cutoff_field == '') {
      drupal_set_message(t("Template submission cut off is not set. Please manually set the cut off dates. Please configure the meeting clone module"), 'warning', FALSE);
    }

    $this->node = $node;
  }

  /**
   * Create the cloned nodes.
   */
  public function run() {

    $now = time();
    $wanted_days = 0;
    // as per hook_form_alter weekly|monthly|bi-monthly
    switch ($this->interval) {
      // If reviewing a populated node, then either display the node or save it.
      case 'weekly':
        $wanted_days = $this->getFutureDatesWeek();
        break;
      case 'monthly':
        $wanted_days = $this->getFutureDatesMonth();
        break;
      case 'bi-monthly':
        $wanted_days = $this->getFutureDatesBiMonth();
        break;
      default:
        // Fail
        watchdog(MEETINGCLONE_MODULE_NAME, 'Clone - unknown interval %interval', array('%interval' => $this->interval), WATCHDOG_ERROR);
    }

    $node_created_count = 0;

    foreach ($wanted_days as $wanted_day) {
      // Create a copy of the template meeting node with date $wanted_day
      $created_node_id = $this->copy_meeting_node($wanted_day);
      if ($created_node_id) {
        $node_created_count++;
      }

      // Find children of the template if the nodehierarchy module has been enabled
      if (module_exists(MEETINGCLONE_NODE_HIERARCHY_MODULE)) {

        // $children are the set of dependent nodes with parent $parent_node
        $children = $this->_nodehierarchy_get_children_menu_links_x($this->node->meetingclone_original);

        // Copy children and recreate nodehierarchy links
        foreach ($children as $child_link) {

          // Load the child node
          $child_node = node_load($child_link[nid]);
          if (!$child_node) {
            watchdog('meetingclone', 'No node found for child node id %nid. Please contact an administrator to set the template node id in the administration section of the site.', array('%nid', $child_link['nid']));
          }
          else {

            // Create a cloned child node
            $created_child_node_id = $this->copy_link_child_node($child_node, $created_node_id);
          }
        }
      }
    }

    // and the pre-existing one.
    $node = $this->node;
    $node->meetingclone_processed = TRUE;

    drupal_set_message("Created " . $node_created_count . ' nodes with serial number: ' . $node->meetingclone_serial, 'status');
  }

  /**
   * Get the children of the given node
   * - obtained from nodehierarchy source
   * @return children nodes
   */
  private function _nodehierarchy_get_children_menu_links_x($pnid, $limit = FALSE) {
    $children = array();

    // Messy - we need specific nodehierarchy content in order to be able to replicate
    $query = db_select('node', 'n');
    $query->fields('menu', array('options', 'link_path'));
    $query->fields('n', array('nid'));
    if ($limit) {
      $query->range(0, $limit);
    }
    HotTopicsNodeHierarchyHelper::ResearchForTopicQueryJoin($pnid, $query);

    $result = $query->execute()->fetchAllAssoc('nid');
    foreach ($result as $item) {
      $item->pnid = $pnid;
      $children[] = $this->_nodehierarchy_prepare_menu_link_x($item);
    }
    return $children;
  }

  /**
   * Helper function to prepare a menu link after it's been loaded.
   *  - obtained from nodehierarchy source
   */
  protected function _nodehierarchy_prepare_menu_link_x($menu_link) {
    if ($menu_link && is_object($menu_link)) {
      $menu_details = array();
      $menu_details['options'] = is_string($menu_link->options) ? unserialize($menu_link->options) : $menu_link->options;
      list(, $menu_details['nid']) = explode('/', $menu_link->link_path);
      $menu_details['pnid'] = $menu_link->pnid;
      $menu_details['link_path'] = $menu_link->link_path;

      //  $menu_link['enabled'] = !$menu_link['hidden'];
    }
    return $menu_details;
  }

  /**
   * Set Weekly set of days
   * @return type 
   */
  protected function getFutureDatesWeek() {
    $wanted_days = array();

    // not going to keep the node that has already been cloned
    for ($i = 0; $i < $this->number; $i++) {
      $new_date = strtotime(date(self::DATE_FORMAT, strtotime($this->start_date)) . " +" . $i . " week");
      $day_of_week = $this->findDayInMonth($new_date);
      $wanted_days[] = $day_of_week;
    }

    return $wanted_days;
  }

  /**
   * Return monthly set of dates
   * 
   * @return type 
   */
  protected function getFutureDatesMonth() {
    $wanted_days = array();

    // not going to keep the node that has already been cloned
    for ($i = 0; $i < $this->number; $i++) {
      $new_date = strtotime(date(self::DATE_FORMAT, strtotime($this->start_date)) . " +" . $i . " month");
      $day_of_week = $this->findDayInMonth($new_date);
      $wanted_days[] = $day_of_week;
    }

    return $wanted_days;
  }

  /**
   * Return a set of days for every second month.
   * @return type 
   */
  protected function getFutureDatesBiMonth() {
    $wanted_days = array();

    $interval_count = 0;

    // not going to keep the node that has already been cloned
    while (count($wanted_days) < $this->number) {
      $months_ahead = 2 * $interval_count;
      $new_date = strtotime(date(self::DATE_FORMAT, strtotime($this->start_date)) . " +" . $months_ahead . " month");
      $timestamp = $this->findDayInMonth($new_date);

      if ($timestamp > time()) {
        $wanted_days[] = $timestamp;
      }

      // We need zero months ahead.
      $interval_count++;
    }

    // Review by echo 1371132000 | gawk '{print strftime("%c", $0)}'
    return $wanted_days;
  }

  /**
   * Find a day in the month based on weekly instances.
   * 
   * Increment the month if the found interval is before.
   * 
   * @param type $wanted_month 
   *   is the timestamp value of the month we want.
   * 
   * @pre
   *   configure $this->instance_in_month to be 1-4 or -1 for last
   *   configure $this->wanted_day_in_week to be 0-6 for the day in the week.
   */
  protected function findDayInMonth($wanted_month) {
    $date_of_interest = $this->findDayInMonthImpl($wanted_month);
    return $date_of_interest;
  }

  /**
   * Do the find next date calculation.
   * 
   * e.g. Find the second friday in the month.
   * 
   * @param timestamp $month the month in which we want to find the event.
   * @return type
   */
  protected function findDayInMonthImpl($wanted_month) {
    // Convenience mapping.
    $month_names = array(0 => "Sun", 1 => "Mon", 2 => "Tue", 3 => "Wed", 4 => "Thu", 5 => "Fri", 6 => "Sat");

    // The actual calculations
    $this_month_timestamp = strtotime(date("Y-m-01", $wanted_month));

    // -1 for last
    $date_of_interest = (-1 == $this->instance_in_month) ?
        strtotime("last " . $month_names[$this->wanted_day_in_week], $this_month_timestamp) :
        strtotime($month_names[$this->wanted_day_in_week] . " + " . ($this->instance_in_month - 1) . " weeks", $this_month_timestamp);

    return $date_of_interest;
  }

  /**
   * Saves a copy of the node for the given day
   * 
   * @global type $user
   * @param type $wanted_day
   *   Date to set this meeting to
   * 
   * @return type 
   */
  protected function copy_meeting_node($wanted_day) {
    $original_node = $this->node;

    // Test that the node is OK to clone and then clone it.
    // This template node is already a cloned node. So doesn't have nid.
    if (isset($original_node->type) && meetingclone_is_permitted($original_node->type)) {
      global $user;

      $new_node = clone $original_node;

      $new_node->nid = NULL;
      $new_node->vid = NULL;
      $new_node->tnid = NULL;
      $new_node->name = $user->name;
      $new_node->uid = $user->uid;
      $new_node->created = NULL;

      $new_node->book['mlid'] = NULL;
      $new_node->path = NULL;
      $new_node->files = array();

      // Mark as not temporary
      $new_node->meetingclone_temponly = FALSE;

      // Add meeting number.
      // Set the date properly. Change by reference.
      $date_field = $this->template_date_field;
      $this->create_date_array($wanted_day, $new_node->$date_field);

      /*
       * Submission cutoff date is three days prior to the meeting date
       */
      if (isset($this->template_submission_cutoff_field) && $this->template_submission_cutoff_field != "") {
        $date_cutoff_field = $this->template_submission_cutoff_field;
        $this->create_date_array(strtotime("- 3 days", $wanted_day), $new_node->$date_cutoff_field);
      }

      // Add an extra property as a flag.
      // Already has the serial numbers. Fail if not.
      $new_node->meetingclone_processed = TRUE;

      if (variable_get('meetingclone_reset_' . $new_node->type, FALSE)) {
        $node_options = variable_get('node_options_' . $new_node->type, array('status', 'promote'));
        // Fill in the default values.
        foreach (array('status', 'moderate', 'promote', 'sticky', 'revision') as $key) {
          $new_node->$key = in_array($key, $node_options);
        }
      }

      // Let other modules do special fixing up.
      // The function signature is: hook_meetingclone_node_alter(&$node, $context)
      // $context is an array with two elements, 'method' and 'original_node',
      // where 'method' is either 'prepopulate' or 'save-edit'.
      $context = array('method' => 'save-edit', 'original_node' => $original_node);
      drupal_alter('meetingclone_node', $new_node, $context);

      /*
       * "INSERT INTO {node_comment_statistics} (nid, cid, last_comment_timestamp, 
       * last_comment_name, last_comment_uid, comment_count) VALUES 
       * (:db_insert_placeholder_0, :db_insert_placeholder_1, :db_insert_placeholder_2, 
       * :db_insert_placeholder_3, :db_insert_placeholder_4, :db_insert_placeholder_5)"
       */

      try {
        node_save($new_node);

        // Stupidly we have to add a record of this node to the database.
      } catch (Exception $e) {
        drupal_set_message("Failed to save node " . $e->getMessage, 'error');
        watchdog_exception(MEETINGCLONE_MODULE_NAME, $e);
        return;
      }

      try {
        $invite_record = new MeetingCloneJournal($new_node->meetingclone_serial, $new_node->nid);
        $write_result = drupal_write_record(MEETINGCLONE_TABLE, $invite_record); // primary_keys, 3rd argument.
      } catch (Exception $e) {
        watchdog_exception(MEETINGCLONE_MODULE_NAME, $e);
      }


      return $new_node->nid;
    }
  }

  /**
   * Saves a copy of the child node
   * 
   * @global type $user
   * @param child_node the meeting's child node to copy
   * 
   * @return type 
   */
  protected function copy_link_child_node($child_node, $pnid) {
    global $user;

    $new_node = clone $child_node;

    $new_node->nid = NULL;
    $new_node->vid = NULL;
    $new_node->tnid = NULL;
    $new_node->name = $user->name;
    $new_node->uid = $user->uid;
    $new_node->path = NULL;
    $new_node->files = array();

    $menu_link = _nodehierarchy_default_menu_link(null, 0, 0);

    // Find the parent menu link id. 
    // The child node, has a parent menu link. This in turn has a parent menu link which points to the node url.
    // See _menu.inc 3094
    $menu_link['pnid'] = $pnid; // 6999
    // link_title should be the node title
    $new_node->nodehierarchy_menu_links = array($menu_link);

    try {
      node_save($new_node);
      error_log('New cloned child node Saved: ' . $new_node->nid);

      // Stupidly we have to add a record of this node to the database.
    } catch (Exception $e) {
      error_log("failed to save child node " . $e->getMessage());
      drupal_set_message("Failed to save child node " . $e->getMessage(), 'error');
      watchdog_exception(MEETINGCLONE_MODULE_NAME, $e);
      return;
    }

    return $new_node->nid;
  }

  /**
   * 
   * @param type $wanted_day 
   *   timestamp of the day the meeting is held on.
   * 
   * @param array $existing_date_field
   *   Currently set date field to use as a template.
   * 
   * @return array of the date, drupal style. 
   */
  protected function create_date_array($wanted_day, &$existing_date_field) {
    // und is $node->language
    $formatted_to_drupal = $this->convert_to_drupal_date($wanted_day);
    $existing_date_field['und'][0]['value'] = $formatted_to_drupal;
    $existing_date_field['und'][0]['value2'] = $formatted_to_drupal;
  }

  /**
   * Used by create_date_array for internal date storage
   * 
   * @param integer $timestamp
   * @return string
   */
  protected function convert_to_drupal_date($timestamp) {
    $format = 'Y-m-d H:i:s';

    $date_time = new DateTime();
    $wanted_date_time = $date_time->setTimestamp($timestamp);


    return date_format($wanted_date_time, $format);
  }

}