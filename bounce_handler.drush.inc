<?php
// $Id$

/**
 * Implementation of hook_drush_help().
 */
function bounce_handler_drush_help($section) {
  switch ($section) {
    case 'symantec-admin:notifications':
      return dt('Run messaging/notifications module cron.');
  }
}

/**
 * Implementation of hook_drush_command().
 */
function bounce_handler_drush_command() {
  return array(
    'bounce' => array(
      'description' => 'Process incoming bounce.',
      'arguments' => array(
        'sender' => 'Sender',
        'recipient' => 'Recipient',
      ),
    ),
  );
}

/**
 * Command callback.
 */
function drush_bounce_handler_bounce() {
  require_once "Mail/mimeDecode.php";
  require_once 'Mail/RFC822.php';
  require_once drupal_get_path('module', 'bounce_handler') .'/phpmailer-bmh_rules.php';

  $incoming_bounce = '';
  $stdin = fopen('php://stdin', 'r'); 
  while (!feof($stdin)) { 
    $incoming_bounce.= fgets($stdin,4096); 
  }
  fclose($stdin);

  $params['include_bodies'] = TRUE;
  $params['decode_bodies'] = TRUE;
  $params['decode_headers'] = TRUE;
  $params['input'] = $incoming_bounce;

  $structure = Mail_mimeDecode::decode($params);
  // If we can't parse the message we discard it.
  if (is_object($structure) && isset($structure->headers) && count($structure->headers) > 1) {
    // The To address is the envelop from address of the original mail.
    $to = Mail_RFC822::parseAddressList($structure->headers['to']);
    // de-verpify
    // TODO: Make VERP strings configurable?
    $destination = explode('+', $to[0]->mailbox, 2);
    $destination = str_replace('=', '@', $destination[1]);

    if ($structure->ctype_primary == 'multipart' && $structure->ctype_secondary == 'report') {
      $result = bmhDSNRules($structure->parts[0]->body, $structure->parts[1]->body);
    }
    else {
      $result = bmhBodyRules($structure->body, '', TRUE);
    }
    // Find the user
    $account = db_fetch_object(db_query("SELECT uid FROM {users} WHERE mail = '%s'", $destination));
    // Now save the message in the DB, increase the counters
    $time = $_SERVER['REQUEST_TIME'];

    $sid = db_result(db_query("SELECT sid FROM {bounce_statistics} WHERE mail = '%s'", $destination));
    if ($sid) {
      db_query("UPDATE {bounce_statistics} SET count = count + 1, remove = remove + %d, timestamp = %d WHERE sid = %d", $result['remove'], $time, $sid);
    }
    else {
      db_query("INSERT INTO {bounce_statistics} SET count = count + 1, remove = remove + %d, timestamp = %d, mail = '%s'", $result['remove'], $time, $destination);
      $sid = db_last_insert_id('bounce_statistics', 'sid');
    }

    db_query("INSERT INTO {bounces} (sid, uid, timestamp, destination, bmh_no, bmh_cat, bmh_type, severity, message) VALUES (%d, %d, %d, '%s', %d, '%s', '%s', %d, '%s')", $sid, isset($account->uid) ? $account->uid : 0, $time, $destination, $result['rule_no'], $result['rule_cat'], $result['bounce_type'], $result['remove'], $incoming_bounce);
  }
}

/**
 * This is a slightly ugly hack in order to by able to use phpmailer-bmh_rules.php unmodified.
 */
if (!function_exists('imap_rfc822_parse_adrlist')) {
  function imap_rfc822_parse_adrlist($address, $default_host) {
    return Mail_RFC822::parseAddressList($address, $default_host);
  }
}

