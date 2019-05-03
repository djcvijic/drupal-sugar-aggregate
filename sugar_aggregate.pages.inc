<?php

/**
 * @file
 * Pages file for sugar_aggregate.
 */

/**
 * Callback for hook_menu().
 * 
 * @return string
 *   The rendered page.
 * 
 * @ingroup callbacks
 */
function sugar_aggregate_index() {
  $output = '<br /><a href="' . url("admin/sugar_aggregate/config") . '">Configure Sugar Aggregate</a><br />';

  if (!variable_get('sugar_aggregate_api_url')
    || !variable_get('sugar_aggregate_username')
    || !variable_get('sugar_aggregate_password_hash')
    || !variable_get('sugar_aggregate_api_protocol')) {
    return $output;
  }

  $users = entity_load('user');

  $header = array(
    array('data' => t('ID'), 'field' => 'id', 'sort' => 'asc'),
    array('data' => t('Title'), 'field' => 'content_title'),
    array('data' => t('Author'), 'field' => 'node_author_uid'),
    array('data' => t('Last Imported'), 'field' => 'imported'),
    array('data' => t('Automatic Import')),
    array('data' => t('Action')),
  );
  
  $query = db_select('sugar_aggregate', 'sa');
  $query->fields('sa', array('id', 'content_title', 'node_author_uid', 'imported', 'cron', 'node_nid'));
  $result = $query->execute();
  
  $rows = array();
  foreach ($result as $res) {
    $cron = json_decode($res->cron);
    $rows[] = array(
      $res->id,
      $res->content_title,
      !empty($users[$res->node_author_uid]->name) ? $users[$res->node_author_uid]->name : '-',
      ($res->imported ? format_date($res->imported) : 'Never'),
      (($cron && $cron->enabled) ? ('Every ' . $cron->periodAmount . ' ' . $cron->periodUnit) : '-'),
      ($res->node_nid ? '<a href="' . url('node/' . $res->node_nid) . '">View imported content</a> | ' : '')
      . '<a href="' . url("admin/sugar_aggregate/$res->id/import") . '">Import</a> | '
      . '<a href="' . url("admin/sugar_aggregate/$res->id/edit") . '">Edit</a> | '
      . '<a href="' . url("admin/sugar_aggregate/$res->id/delete") . '" onclick="return confirm(\'Are you sure?\')">Delete</a>',
    );
  }

  if (!empty($rows)) {
    $output .= '<br />' . theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'sort-table'))) . '<br />';
  }
  else {
    $output .= '<br />' . t('No results found.') . '<br />';
  }
  $output .= '<br /><a href="' . url("admin/sugar_aggregate/add") . '">Add new record</a><br />';

  return $output;
}

/**
 * Form constructor for the Sugar Aggregate config form.
 * 
 * @see sugar_aggregate_config_form_submit()
 * 
 * @ingroup forms
 */
function sugar_aggregate_config_form($form, &$form_state) {
  $form = array();
  $form['#redirect'] = 'admin/sugar_aggregate';
  $form['#sugar_password_hash'] = variable_get('sugar_aggregate_password_hash', '');
  $form['sugar_api_url'] = array(
    '#type' => 'textfield',
    '#title' => 'Sugar API URL',
    '#description' => 'Full URL of the SugarCRM instance (e.g. http://www.mydomain.com/sugarcrm)',
    '#required' => TRUE,
    '#value' => variable_get('sugar_aggregate_api_url', ''),
  );
  $form['username'] = array(
    '#type' => 'textfield',
    '#title' => 'Sugar Username',
    '#description' => 'This user must have access to authenticate via the API and read module entries',
    '#required' => TRUE,
    '#value' => variable_get('sugar_aggregate_username', ''),
  );
  $form['password'] = array(
    '#type' => 'password',
    '#title' => 'Sugar Password',
    '#required' => TRUE,
  );
  $form['sugar_api_protocol'] = array(
    '#type' => 'select',
    '#title' => 'Sugar API Protocol',
    '#description' => 'Protocol to use to communicate with SugarCRM, CustomSoap may offer a smoother integration than Rest but may not be available.',
    '#required' => TRUE,
    '#value' => variable_get('sugar_aggregate_api_protocol', 'Rest'),
    '#default_value' => 'Rest',
    '#options' => array('Rest' => 'Rest', 'CustomSoap' => 'CustomSoap'),
  );
  $form['disable_javascript'] = array(
    '#type' => 'checkbox',
    '#title' => 'Disable Javascript',
    '#description' => 'If there is a problem with the javascript on the Sugar Aggregate add/edit pages, this is the kill switch.',
    '#value' => variable_get('sugar_aggregate_disable_javascript', FALSE),
  );

  return confirm_form($form, t('Configure Sugar Aggregate'), 'admin/sugar_aggregate', '', t('Submit'), t('Cancel'), 'confirm sugar_aggregate_config update');
}

/**
 * Form submission handler for the Sugar Aggregate config form.
 */
function sugar_aggregate_config_form_submit($form, &$form_state) {
  $form_state['redirect'] = $form['#redirect'];
  $old_password_hash = $form['#sugar_password_hash'];
  variable_set('sugar_aggregate_api_url', $form_state['input']['sugar_api_url']);
  variable_set('sugar_aggregate_username', $form_state['input']['username']);
  variable_set('sugar_aggregate_password_hash', $form_state['input']['password'] ? md5($form_state['input']['password']) : $old_password_hash);
  variable_set('sugar_aggregate_api_protocol', $form_state['input']['sugar_api_protocol']);
  variable_set('sugar_aggregate_disable_javascript', $form_state['input']['disable_javascript']);
  drupal_set_message(t('Sugar Aggregate configured!'));
}

/**
 * Form constructor for the Sugar Aggregate  form.
 * 
 * @param int $sugar_aggregate_id
 *   Id of the Sugar Aggregate.
 * 
 * @see sugar_aggregate_form_submit()
 * 
 * @ingroup forms
 */
function sugar_aggregate_form($form, &$form_state, $sugar_aggregate_id) {
  global $user;
  $record = NULL;
  if ($sugar_aggregate_id) {
    $record = db_query('SELECT * FROM {sugar_aggregate} WHERE id = :id', array(':id' => $sugar_aggregate_id))
      ->fetchObject();
  }

  $sugar_query = (!empty($record->sugar_query) ? json_decode($record->sugar_query) : NULL);

  // If we are rendering the form (GET request),
  // have previously entered a module_name,
  // and javascript is enabled,
  // then fetch metadata about related links and fields,
  // and use it to help in filling out the rest of the form.
  $disable_javascript = variable_get('sugar_aggregate_disable_javascript', FALSE);
  $sugar_metadata = NULL;
  if (empty($form_state['input']) && !empty($sugar_query->module_name) && !$disable_javascript) {
    module_load_include('inc', 'sugar_aggregate', 'includes/SugarCommunicator');
    $communicator = SugarCommunicator::instance();
    $sugar_metadata = $communicator->getMetadata($record);
  }

  $bundles = array_keys(field_info_bundles('node'));
  $bundles = array_combine($bundles, $bundles);

  $languages = locale_language_list();

  $users = entity_load('user');
  $users = array_filter($users, function($u) { return $u->status; });
  $users = array_map(function($u) { return $u->name; }, $users);

  $cron = (!empty($record->cron) ? json_decode($record->cron) : NULL);

  $form = array();
  $form['#attached']['css'] = array(
    drupal_get_path('module', 'sugar_aggregate') . '/css/bootstrap-forms.css',
    drupal_get_path('module', 'sugar_aggregate') . '/css/bootstrap-typeahead.css',
    drupal_get_path('module', 'sugar_aggregate') . '/css/bootstrap-tokenfield-typeahead.min.css',
    drupal_get_path('module', 'sugar_aggregate') . '/css/bootstrap-tokenfield.min.css',
    drupal_get_path('module', 'sugar_aggregate') . '/css/sugar_aggregate.css',
  );
  if (!$disable_javascript) {
    $form['#attached']['js'] = array(
      drupal_get_path('module', 'sugar_aggregate') . '/js/typeahead.bundle.min.js',
      drupal_get_path('module', 'sugar_aggregate') . '/js/bootstrap-tokenfield.min.js',
      drupal_get_path('module', 'sugar_aggregate') . '/js/jquery.asuggest.js',
      drupal_get_path('module', 'sugar_aggregate') . '/js/jquery.a-tools-1.4.1.js',
      drupal_get_path('module', 'sugar_aggregate') . '/js/sugar_aggregate.js',
    );
  }
  $form['#attributes'] = array(
    'class' => array('sugar-aggregate-form'),
    'data-sugar-metadata' => json_encode($sugar_metadata),
  );
  $form['#redirect'] = 'admin/sugar_aggregate';
  $form['#sugar_aggregate_id'] = $sugar_aggregate_id;
  $form['sugar_query'] = array(
    '#tree' => TRUE,
    'module_name' => array(
      '#type' => 'textfield',
      '#attributes' => array('class' => array('module-name-input')),
      '#title' => 'Sugar Module Name',
      '#description' => 'The base module for which information is being fetched (e.g. Leads)',
      '#required' => TRUE,
      '#value' => ($sugar_aggregate_id && $sugar_query ? $sugar_query->module_name : ''),
    ),
    'module_fields' => array(
      '#type' => 'textfield',
      '#attributes' => array('class' => array('module-fields-input')),
      '#title' => 'Sugar Module Fields',
      '#description' => 'A comma-separated list of fields to fetch for the module above (e.g. id,last_name,phone_work)',
      '#required' => TRUE,
      '#value' => ($sugar_aggregate_id && $sugar_query ? $sugar_query->module_fields : ''),
    ),
    'link_name' => array(
      '#type' => 'textfield',
      '#attributes' => array('class' => array('link-name-input')),
      '#title' => 'Sugar Link Name',
      '#description' => 'An extra module linked to the module above (e.g. email_addresses)',
      '#required' => FALSE,
      '#value' => ($sugar_aggregate_id && $sugar_query ? $sugar_query->link_name : ''),
    ),
    'link_fields' => array(
      '#type' => 'textfield',
      '#attributes' => array('class' => array('link-fields-input')),
      '#title' => 'Sugar Link Fields',
      '#description' => 'A comma-separated list of fields to fetch for the linked module (e.g. id,primary_address,opt_out)',
      '#required' => FALSE,
      '#value' => ($sugar_aggregate_id && $sugar_query ? $sugar_query->link_fields : ''),
    ),
  );
  $form['skip_conditions'] = array(
    '#type' => 'textarea',
    '#attributes' => array('class' => array('skip-conditions-input')),
    '#title' => 'Skip Conditions',
    '#description' => 'One condition per line, in the form module.field==123 (comparators: == and !=), if ANY condition is met for an item, it is skipped (e.g. email_addresses.opt_out==1)',
    '#required' => FALSE,
    '#value' => ($sugar_aggregate_id ? $record->skip_conditions : ''),
  );
  $form['content_title'] = array(
    '#type' => 'textfield',
    '#title' => 'Content Title',
    '#description' => 'Title for the imported content',
    '#required' => TRUE,
    '#value' => ($sugar_aggregate_id ? $record->content_title : ''),
  );
  $form['content_summary'] = array(
    '#type' => 'textarea',
    '#title' => 'Content Summary',
    '#description' => 'Summary for the imported content',
    '#required' => FALSE,
    '#value' => ($sugar_aggregate_id ? $record->content_summary : ''),
  );
  $form['content_template'] = array(
    '#type' => 'textarea',
    '#attributes' => array('class' => array('content-template-input')),
    '#title' => 'Content Template',
    '#description' => 'Main template for the imported content, use {{loop}} to mark where looped items should be injected, {{time}} to print the time of import',
    '#required' => TRUE,
    '#value' => ($sugar_aggregate_id ? $record->content_template : ''),
  );
  $form['loop_template'] = array(
    '#type' => 'textarea',
    '#attributes' => array('class' => array('loop-template-input')),
    '#title' => 'Loop Template',
    '#description' => 'Template which will be rendered once for each fetched item, use {{MODULE.FIELD}} to print their values (e.g. {{Leads.last_name}})',
    '#required' => TRUE,
    '#value' => ($sugar_aggregate_id ? $record->loop_template : ''),
  );
  $form['node_type'] = array(
    '#type' => 'select',
    '#title' => 'Node Type',
    '#description' => 'Node type for the imported content, page is tested and recommended',
    '#required' => TRUE,
    '#value' => ($sugar_aggregate_id ? $record->node_type : 'page'),
    '#default_value' => 'page',
    '#options' => $bundles,
  );
  $form['node_language'] = array(
    '#type' => 'select',
    '#title' => 'Node Language',
    '#description' => 'Language for the associated node, Language neutral is tested and recommended',
    '#value' => ($sugar_aggregate_id ? $record->node_language : LANGUAGE_NONE),
    '#empty_option' => 'Language neutral',
    '#empty_value' => LANGUAGE_NONE,
    '#default_value' => LANGUAGE_NONE,
    '#options' => $languages,
  );
  $form['node_author_uid'] = array(
    '#type' => 'select',
    '#title' => 'Node Author',
    '#description' => 'Associated node will be created/edited as this user, authentication will be done so they must have permissions to create/edit this node',
    '#required' => TRUE,
    '#value' => ($sugar_aggregate_id ? $record->node_author_uid : $user->uid),
    '#default_value' => $user->uid,
    '#options' => $users,
  );
  $form['node_path_alias'] = array(
    '#type' => 'textfield',
    '#title' => 'Node Path Alias',
    '#description' => 'Path alias to use for the URL of the attached node (e.g. "foo/bar"), by default Content Title is used',
    '#required' => FALSE,
    '#value' => ($sugar_aggregate_id ? $record->node_path_alias : ''),
  );
  $form['cron'] = array(
    '#tree' => TRUE,
    'cronLabel' => array(
      '#markup' => '<div><label>Automatic Import</label></div>',
    ),
    'enabled' => array(
      '#type' => 'checkbox',
      '#attributes' => array('class' => array('cron-enabled-input')),
      '#title' => 'Enable',
      '#value' => ($sugar_aggregate_id && $cron ? $cron->enabled : FALSE),
    ),
    'periodLabel' => array(
      '#markup' => '<div class="form-item-cron-periodLabel">Import once every</div>',
    ),
    'periodAmount' => array(
      '#type' => 'textfield',
      '#attributes' => array('class' => array('cron-period-amount-input')),
      '#required' => TRUE,
      '#value' => ($sugar_aggregate_id && $cron ? $cron->periodAmount : '1'),
    ),
    'periodUnit' => array(
      '#type' => 'select',
      '#attributes' => array('class' => array('cron-period-unit-input')),
      '#required' => TRUE,
      '#value' => ($sugar_aggregate_id && $cron ? $cron->periodUnit : 'days'),
      '#default_value' => 'days',
      '#options' => array('days' => 'days', 'hours' => 'hours'),
    ),
  );

  return confirm_form($form, t('Sugar Aggregate Form'), 'admin/sugar_aggregate', '', t('Submit'), t('Cancel'), 'confirm sugar_aggregate update');
}

/**
 * Form submission handler for the Sugar Aggregate form.
 */
function sugar_aggregate_form_submit($form, &$form_state) {
  $form_state['redirect'] = $form['#redirect'];
  $sugar_aggregate_id = $form['#sugar_aggregate_id'];
  if ($sugar_aggregate_id) {
    $count = db_update('sugar_aggregate')
      ->fields(array(
        'sugar_query' => ($form_state['input']['sugar_query'] ? json_encode($form_state['input']['sugar_query']) : ''),
        'skip_conditions' => $form_state['input']['skip_conditions'],
        'content_title' => $form_state['input']['content_title'],
        'content_summary' => $form_state['input']['content_summary'],
        'content_template' => $form_state['input']['content_template'],
        'loop_template' => $form_state['input']['loop_template'],
        'node_type' => $form_state['input']['node_type'],
        'node_language' => $form_state['input']['node_language'],
        'node_author_uid' => $form_state['input']['node_author_uid'],
        'node_path_alias' => $form_state['input']['node_path_alias'],
        'cron' => ($form_state['input']['cron'] ? json_encode($form_state['input']['cron']) : ''),
      ))
      ->condition('id', $sugar_aggregate_id, '=')
      ->execute();
    drupal_set_message(t('Sugar Aggregate modified!'));
  }
  else {
    $sugar_aggregate_id = db_insert('sugar_aggregate')
      ->fields(array(
        'sugar_query' => ($form_state['input']['sugar_query'] ? json_encode($form_state['input']['sugar_query']) : ''),
        'skip_conditions' => $form_state['input']['skip_conditions'],
        'content_title' => $form_state['input']['content_title'],
        'content_summary' => $form_state['input']['content_summary'],
        'content_template' => $form_state['input']['content_template'],
        'loop_template' => $form_state['input']['loop_template'],
        'node_type' => $form_state['input']['node_type'],
        'node_language' => $form_state['input']['node_language'],
        'node_author_uid' => $form_state['input']['node_author_uid'],
        'node_path_alias' => $form_state['input']['node_path_alias'],
        'cron' => ($form_state['input']['cron'] ? json_encode($form_state['input']['cron']) : ''),
      ))
      ->execute();
    drupal_set_message(t('Sugar Aggregate added!'));
  }
}

/**
 * Callback for hook_menu().
 * 
 * @param int $sugar_aggregate_id
 *   Id of the Sugar Aggregate.
 * 
 * @ingroup callbacks
 */
function sugar_aggregate_import($sugar_aggregate_id) {
  module_load_include('inc', 'sugar_aggregate', 'includes/SugarCommunicator');
  module_load_include('inc', 'sugar_aggregate', 'includes/SugarAggregator');
  $communicator = SugarCommunicator::instance();
  $aggregator = new SugarAggregator();

  $record = db_query('SELECT * FROM {sugar_aggregate} WHERE id = :id', array(':id' => $sugar_aggregate_id))
    ->fetchObject();

  $content_body = $communicator->import($record);
  if (empty($content_body)) {
    drupal_set_message(t('Sugar Aggregate import failed!'));
    drupal_goto('admin/sugar_aggregate');
    return;
  }

  $node_id = $aggregator->process($record, $content_body, TRUE);

  drupal_set_message(t('Sugar Aggregate imported!'));
  drupal_goto('admin/sugar_aggregate');
}

/**
 * Callback for hook_menu().
 * 
 * @param int $sugar_aggregate_id
 *   Id of the Sugar Aggregate.
 * 
 * @ingroup callbacks
 */
function sugar_aggregate_delete($sugar_aggregate_id) {
  db_delete('sugar_aggregate')
    ->condition('id', $sugar_aggregate_id, '=')
    ->execute();
  drupal_set_message(t('Sugar Aggregate deleted!'));
  drupal_goto('admin/sugar_aggregate');
}
