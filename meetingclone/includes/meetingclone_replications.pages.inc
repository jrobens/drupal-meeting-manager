<?php

/**
 * @file
 *  Manage the list of replication activities. 
 * 
 * jrobens@interlated.com.au 201205
 */

/**
 * Apply filters for node administration filters based on session.
 *
 * @param $query
 *   A SelectQuery to which the filters should be applied.
 */
function meetingclone_build_filter_query(SelectQueryInterface $query) {
  // Build query
  $filter_data = isset($_SESSION['node_overview_filter']) ? $_SESSION['node_overview_filter'] : array();
  foreach ($filter_data as $index => $filter) {
    list($key, $value) = $filter;
    switch ($key) {
      case 'status':
        // Note: no exploitable hole as $key/$value have already been checked when submitted
        list($key, $value) = explode('-', $value, 2);
      case 'type':
      case 'language':
        $query->condition('n.' . $key, $value);
        break;
    }
  }
}

/**
 * List all the replications found on the system. 
 * 
 * Sortable table with dialog to display the details for a replication.
 */
function meetingclone_list_replication_form($form, &$form_state) {
  // List of replications 
  $replications = new MeetingReplications();

  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Show only items where'),
    '#theme' => 'exposed_filters__node',
  );

  $form['replications'] = array(
    '#type' => 'fieldset',
    '#title' => t('Replications'),
  );

  $form['replications']['get'] = array(
    '#title' => t('Previous Replications'),
    '#type' => 'tableselect',
    '#header' => $replications->buildSortHeader(),
    '#options' => $replications->get(),
    '#default_value' => '',
    '#description' => t('List replications in the system.'),
  );

  $form['pager'] = array('#markup' => theme('pager'));

  // Trigger a delete
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Delete',
  );


  drupal_add_js('misc/form.js');

  drupal_set_title('Replications');

  return $form;
}

/**
 * Submit function for replications filter form. 
 * 
 * See node.admin.inc
 * 
 * @param type $form
 * @param type $form_state
 */
function meetingclone_list_replication_form_submit($form, &$form_state) {
  if (!empty($form_state['values']['op']) && $form_state['values']['op'] == "Delete") {
    _meetingclone_delete_replication($form_state);
    return TRUE;
  }

  /* Filter options from node.admin.inc
    $filters = node_filters();
    switch ($form_state['values']['op']) {
    case t('Filter'):
    case t('Refine'):
    // Apply every filter that has a choice selected other than 'any'.
    foreach ($filters as $filter => $options) {
    if (isset($form_state['values'][$filter]) && $form_state['values'][$filter] != '[any]') {
    // Flatten the options array to accommodate hierarchical/nested options.
    $flat_options = form_options_flatten($filters[$filter]['options']);
    // Only accept valid selections offered on the dropdown, block bad input.
    if (isset($flat_options[$form_state['values'][$filter]])) {
    $_SESSION['node_overview_filter'][] = array($filter, $form_state['values'][$filter]);
    }
    }
    }
    break;
    case t('Undo'):
    array_pop($_SESSION['node_overview_filter']);
    break;
    case t('Reset'):
    $_SESSION['node_overview_filter'] = array();
    break;
    }
   * 
   */
}

/**
 * Delete replications marked as submitted for deletion.
 */
function _meetingclone_delete_replication(array $form_state) {
  if (empty($form_state['values']['get'])) {
    watchdog(MEETINGCLONE_MODULE_NAME, "Deletion specified, but no [values][get]. Check the form.");
    return FALSE;
  }

  foreach ($form_state['values']['get'] as $replication_entry) {
    // Only delete selected items.
    if ($replication_entry) {
      $replication = new MeetingCloneReplication($replication_entry);
      $replication->delete();
    }
  }
}

/**
 * Encapsulate a list of replications. That is an index of the replications that have been run.
 */
class MeetingReplications {

  protected $replications = array();

  public function __construct() {
    $this->find();
  }

  /**
   * 
   * @return array
   *   the list of replications. 
   */
  public function get() {
    return $this->replications;
  }

  /**
   * Find replications. Initialises the class variable. 
   * 
   * @return  
   */
  private function find() {
    // Try/catch? error checking.

    $query = db_select('meetingclone_history', 'mh')->extend('PagerDefault')->extend('TableSort');
    //node_build_filter_query($query);
    // Find a list of all the replications. Each replications has a set of replicated nodes that 
    // We can find in a second step.
    $repids = $query
        ->fields('mh', array('repid', 'nid', 'created'))
        ->limit(50)
        ->orderByHeader($this->buildSortHeader())
        //   ->addTag('node_access')
        ->distinct()
        ->execute()
        ->fetchAllAssoc('repid');

    foreach ($repids as $replication) {
      if (!empty($replication->repid)) {
        $this->replications[$replication->repid] = $this->_buildReplicationOption($replication);
      }
    }

    return TRUE;
  }

  /**
   * Build the header for the tableselect form element.
   * 
   * @return type
   */
  public function buildSortHeader() {
    // Build the sortable table header.
    $header = array(
      'repid' => t('Replication ID'),
      'nid' => t('Node ID'),
      'created' => array('data' => t('Created'), 'field' => 'mh.created', 'sort' => 'desc')
    );
    //$header['operations'] = array('data' => t('Operations'));
    return $header;
  }

  /**
   * Build an option list for the tableselect values.
   * 
   * @param array $replication
   * @return type
   */
  private function _buildReplicationOption(stdClass $replication) {

    // needs simpledialog
    if (module_exists('simple_dialog')) {
      $option = array();

      $dialog_args = array(
        // required
        'text' => check_plain($replication->repid),
        'path' => 'admin/config/meetingclone/settings/replicants/' . $replication->repid,
        'selector' => 'meetingclone-list-replication-form',
        'title' => 'Replication ' . check_plain($replication->repid),
        // optional
        'options' => array(
          'width' => 900,
        //'resizable' => FALSE,
        //'position' => 'center', // Position can be a string or:
        //'position' => array(60, 'top') // can be an array of xy values
        ),
       // 'class' => array('class-name-1', 'class-name-2'),
      );


      $option['repid'] = theme('simple_dialog_link', $dialog_args);
      $option['nid'] = check_plain($replication->nid);
      $option['created'] = format_date($replication->created, 'short');
      return $option;
    }
    else {
      watchdog(MEETINGCLONE_MODULE_NAME, "module simple_dialog is not installed. We won't be able to show the replicatated nodes.");
    }
  }

}

