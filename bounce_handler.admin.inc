<?php
// $Id$

function bounce_handler_admin_settings() {
  $form['bounce_handler_row_limit'] = array(
    '#type' => 'select',
    '#title' => t('Discard bounce entries above the following row limit'),
    '#default_value' => variable_get('bounce_handler_row_limit', 1000),
    '#options' => drupal_map_assoc(array(100, 1000, 10000, 100000, 1000000)),
    '#description' => t('The maximum number of rows to keep in the bounce log database. Older entries will be automatically discarded. (Requires a correctly configured <a href="@cron">cron maintenance task</a>.)', array('@cron' => url('admin/reports/status')))
  );

  return system_settings_form($form);
}

function bounce_handler_overview() {
  $severity = bounce_handler_severity_levels();
  $categories = bounce_handler_categories();
  $types = bounce_handler_bounce_types();

  $header = array(
    array('data' => t('Type'), 'field' => 'b.bmh_type'),
    array('data' => t('Category'), 'field' => 'b.bmh_cat'),
    array('data' => t('Date'), 'field' => 'b.bid', 'sort' => 'desc'),
    array('data' => t('User'), 'field' => 'u.name'),
    array('data' => t('Operations')),
  );

  $sql = "SELECT b.bid, b.sid, b.uid, b.severity, b.bmh_type, b.bmh_cat, b.timestamp, u.name FROM {bounces} b INNER JOIN {users} u ON b.uid = u.uid";
  $tablesort = tablesort_sql($header);
  $result = pager_query($sql . $tablesort, 50);

  $rows = array();
  while ($bounces = db_fetch_object($result)) {
    $rows[] = array(
      $types[$bounces->bmh_type],
      $categories[$bounces->bmh_cat],
      format_date($bounces->timestamp, 'small'),
      theme('username', $bounces),
      l(t('See bounce'), 'admin/reports/bounce/'. $bounces->bid) .' '. l(t('See all bounces'), 'admin/reports/bounce_mail/'. $bounces->sid),
    );
  }
  if (!$rows) {
    $rows[] = array(array('data' => t('No bounced messages available.'), 'colspan' => 6));
  }

  return theme('table', $header, $rows);
}

function bounce_handler_top() {
  $header = array(
    array('data' => t('Mail')),
    array('data' => t('Count (Total)'), 'field' => 'bounce_count',),
    array('data' => t('Hard bounces'), 'field' => 'hard_bounces', 'sort' => 'desc'),
    array('data' => t('Last bounce'), 'field' => 'b.sid'),
    array('data' => t('User')),
    array('data' => t('Operations')),
  );

  $sql = "SELECT bs.*, u.uid, u.name, SUM(bs.count) AS bounce_count, SUM(bs.remove) AS hard_bounces FROM {bounce_statistics} bs LEFT JOIN {users} u ON bs.mail = u.mail GROUP BY bs.sid, u.uid";
  $tablesort = tablesort_sql($header);

  $result = pager_query($sql . $tablesort, 50);
  $rows = array();
  while ($bounces = db_fetch_object($result)) {
    $rows[] = array(
      check_plain($bounces->mail),
      $bounces->bounce_count,
      $bounces->hard_bounces,
      format_date($bounces->timestamp, 'small'),
      theme('username', $bounces),
      l(t('See bounces'), 'admin/reports/bounce_mail/'. $bounces->sid),
    );
  }
  if (!$rows) {
    $rows[] = array(array('data' => t('No bounced messages available.'), 'colspan' => 6));
  }

  return theme('table', $header, $rows);
}

function bounce_handler_view_bounce($bid) {
  $severity = bounce_handler_severity_levels();
  $categories = bounce_handler_categories();
  $types = bounce_handler_bounce_types();
  $output = '';

  $result = db_query('SELECT b.*, u.name, u.uid FROM {bounces} b LEFT JOIN {users} u ON b.uid = u.uid WHERE b.bid = %d', $bid);

  if ($bounce = db_fetch_object($result)) {
    $rows = array(
      array(
        array('data' => t('Destination'), 'header' => TRUE),
        check_plain($bounce->destination),
      ),
      array(
        array('data' => t('Date'), 'header' => TRUE),
        format_date($bounce->timestamp, 'large'),
      ),
      array(
        array('data' => t('User'), 'header' => TRUE),
        theme('username', $bounce),
      ),
      array(
        array('data' => t('BMH No.'), 'header' => TRUE),
        $bounce->bmh_no,
      ),
      array(
        array('data' => t('Category'), 'header' => TRUE),
        $categories[$bounce->bmh_cat],
      ),
      array(
        array('data' => t('Type'), 'header' => TRUE),
        $types[$bounce->bmh_type],
      ),
      array(
        array('data' => t('Severity'), 'header' => TRUE),
        $severity[$bounce->severity],
      ),
      array(
        array('data' => t('Message'), 'header' => TRUE),
        _bounce_handler_format_message($bounce->message),
      ),
      array(
        array('data' => t('Operations'), 'header' => TRUE),
        l(t('See all bounces'), 'admin/reports/bounce_mail/'. $bounce->sid),
      ),
    );

    $attributes = array('class' => 'bounce');
    $output = theme('table', array(), $rows, $attributes);
  }

  return $output;
}

function bounce_handler_view_bounce_address($sid) {
  $severity = bounce_handler_severity_levels();
  $categories = bounce_handler_categories();
  $types = bounce_handler_bounce_types();
  $output = '';

  $header = array(
    array('data' => t('Address')),
    array('data' => t('Bounce category')),
    array('data' => t('Bounce type')),
    array('data' => t('Severity')),
    array('data' => t('User')),
    array('data' => t('Operations')),
  );

  $result = db_query('SELECT b.*, u.name, u.uid FROM {bounces} b LEFT JOIN {users} u ON b.uid = u.uid WHERE b.sid = %d ORDER BY b.bid DESC', $sid);

  $rows = array();
  while ($bounce = db_fetch_object($result)) {
    $rows[] = array(
      check_plain($bounce->destination),
      $categories[$bounce->bmh_cat],
      $types[$bounce->bmh_type],
      $severity[$bounce->severity],
      theme('username', $bounce),
      l('Disable mail', 'user/' . $bounce->uid . '/bounce_handler_mail_disable'),
    );
  }
  $output = theme('table', $header, $rows);

  return $output;  
}

/**
 * Ideally, this would use a hook...
 */
function bounce_handler_disable_mails(&$form_state, $account) {
  $form = array();
  $form['_account'] = array('#type' => 'value', '#value' => $account);

  $options = array();
  $description = array();
  if (module_exists('contact')) {
    $account = user_load($account->uid);
    if (!empty($account->contact)) {
      $options['contact'] = t('Disable contact form');
      $description['contact'] = t('Contact: The user will be able to reenable this.');
    }
  }
  if (module_exists('notifications')) {
    $count = db_result(db_query("SELECT COUNT(*) AS count FROM {notifications} WHERE uid = %d AND status = %d", $account->uid, NOTIFICATIONS_SUBSCRIPTION_ACTIVE));
    if ($count) {
      $options['notifications'] = t('Disable %notifications', array('%notifications' => format_plural($count, '1 notification', '@count notifications')));
      $description['notifications'] = t('Notifications: The user will be able to reenable his notifications.');
    }
  }
  if (module_exists('project_issue')) {
    $options['project_issue'] = t('Disable issue notifications');
    $description['project_issue'] = t("Project issues: The user's selected issue subscriptions will be lost. Use with care!");
  }

  $form['disable'] = array(
    '#title' => t('Disable selected mail options'),
    '#type' => 'checkboxes',
    '#options' => $options,
  );

  return confirm_form($form,
                      t('Are you sure you want to disable the selected mail categories for the account %name?', array('%name' => $account->name)),
                      'user/'. $account->uid,
                      implode(', ', $description),
                      t('Disable'), t('Cancel'));
}

/**
 * Submit function for the confirm form for user deletion.
 */
function bounce_handler_disable_mails_submit($form, &$form_state) {
  $account = $form_state['values']['_account'];

  if (isset($form_state['values']['disable'])) {
    foreach (array_keys($form_state['values']['disable']) as $module) {
      switch ($module) {
        case 'contact':
          $account = user_load($account->uid);
          user_save($account, array('contact' => NULL));
          break;
        case 'notifications':
          db_query('UPDATE {notifications} SET status = %d WHERE status = %d AND uid = %d', NOTIFICATIONS_SUBSCRIPTION_INACTIVE, NOTIFICATIONS_SUBSCRIPTION_ACTIVE, $account->uid);
          notifications_queue_clean(array('uid' => $account->uid));
          break;
        case 'project_issue':
          db_query('UPDATE {project_subscriptions} SET level = 0 WHERE uid = %d', $account->uid);
          break;
      }
    }
    // Remove bounces for this address and statistics.
    $sid = db_result(db_query("SELECT sid FROM {bounce_statistics} WHERE mail = '%s'", $account->mail));
    db_query("DELETE FROM {bounces} WHERE sid = %d", $sid);
    db_query("DELETE FROM {bounce_statistics} WHERE sid = %d", $sid);
  }

  drupal_set_message(t("%name's mail settings have been disabled.", array('%name' => $form_state['values']['_account']->name)));

  if (!isset($_REQUEST['destination'])) {
    $form_state['redirect'] = 'admin/reports/bounce_handler';
  }
}

function bounce_handler_categories() {
  require_once drupal_get_path('module', 'bounce_handler') .'/phpmailer-bmh_rules.php';
  global $rule_categories;
  return array_combine(array_keys($rule_categories), array_map('t', array_keys($rule_categories)));
}

function bounce_handler_severity_levels() {
  return array(0 => t("don't remove yet"), 1 => t('remove'));
}

function bounce_handler_bounce_types() {
  require_once drupal_get_path('module', 'bounce_handler') .'/phpmailer-bmh_rules.php';
  global $rule_categories;
  $types = array();
  foreach ($rule_categories as $category) {
    $types[$category['bounce_type']] = t($category['bounce_type']);
  }
  return $types;
}

function _bounce_handler_format_message($message) {
  return '<pre>' . filter_xss_admin($message) . '</pre>';
}

