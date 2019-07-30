<?php

/**
 * @file
 * Contains SugarCommunicator, SugarCommunicatorRest,
 *   and SugarCommunicatorCustomSoap class definitions.
 */

/**
 * Calls the SugarCRM API, fetches either metadata or the full content,
 *   and parses the result into the desired format.
 *   The communication protocol is abstracted.
 * 
 * @see \SugarCommunicatorRest
 * @see \SugarCommunicatorCustomSoap
 */
abstract class SugarCommunicator {
  /**
   * SugarCRM API login method.
   * 
   * @var string METHOD_LOGIN
   */
  const METHOD_LOGIN = 'login';

  /**
   * SugarCRM API get_entry_list method.
   * 
   * @var string METHOD_GET_ENTRY_LIST
   */
  const METHOD_GET_ENTRY_LIST = 'get_entry_list';

  /**
   * SugarCRM API get_module_fields method.
   * 
   * @var string METHOD_GET_MODULE_FIELDS
   */
  const METHOD_GET_MODULE_FIELDS = 'get_module_fields';

  /**
   * SugarCRM API logout method.
   * 
   * @var string METHOD_LOGOUT
   */
  const METHOD_LOGOUT = 'logout';

  /**
   * SugarCRM API paging limit.
   *   Note: API will never return more than 20 results.
   * 
   * @var int MAX_RESULTS
   */
  const MAX_RESULTS = 20;

  /**
   * The pattern to match when inserting module field values
   *   into the loop template.
   * 
   * @var string PLACEHOLDER_REGEX
   */
  const PLACEHOLDER_REGEX = '/\{\{(([^.]+)\.([^}]+))\}\}/';

  /**
   * The pattern to match when inserting the loop
   *   into the body template.
   * 
   * @var string LOOP_PLACEHOLDER
   */
  const LOOP_PLACEHOLDER = '{{loop}}';

  /**
   * The equality comparator used in the skip conditions.
   * 
   * @var string COMP_EQ
   */
  const COMP_EQ = '==';

  /**
   * The inequality comparator used in the skip conditions.
   * 
   * @var string COMP_NEQ
   */
  const COMP_NEQ = '!=';

  /**
   * Protected constructor, use instance() instead.
   */
  protected function __construct() {}

  /**
   * Create and return an instance of SugarCommunicator,
   *   based on the 'sugar_aggregate_api_protocol' persistent variable.
   *
   * @return SugarCommunicator|null
   *   The created instance, or null if a bad API protocol is selected.
   */
  public static function instance() {
    $apiProtocol = variable_get('sugar_aggregate_api_protocol', 'Rest');
    switch ($apiProtocol) {
      case 'Rest': return new SugarCommunicatorRest();
      case 'CustomSoap': return new SugarCommunicatorCustomSoap();
      default:
        $this->cleanLog('bad api protocol selected: ' . $apiProtocol);
        return NULL;
    }
  }

  /**
   * Returns the relative path which when appended to the base SugarCRM URL,
   *   constitutes the full API URL to call.
   * 
   * @return string
   *   The suffix.
   */
  abstract protected function getUrlSuffix();

  /**
   * Performs login call to the SugarCRM API, and returns the session Id.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * 
   * @return string|null
   *   The SugarCRM session Id, or null if the call fails.
   */
  abstract protected function login($sugarAggregate);

  /**
   * Performs get_module_fields call to the SugarCRM API,
   *   and returns metadata about the passed module's fields and links
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * @param string $sessionId
   *   The SugarCRM session Id.
   * @param string $moduleName
   *   The name of the module in the SugarCRM system.
   * 
   * @return array
   *   Associative array of metadata, or empty array if something fails.
   */
  abstract protected function getModuleFields($sugarAggregate, $sessionId, $moduleName);

  /**
   * Performs multiple get_entry_list calls to the SugarCRM API,
   *   and returns the merged result of module and relationship entries.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * @param string $sessionId
   *   The SugarCRM session Id.
   * 
   * @return array
   *   Associative array of module and relationship entries,
   *   or empty array if something fails.
   */
  abstract protected function getEntryList($sugarAggregate, $sessionId);

  /**
   * Performs logout call to the SugarCRM API.
   *
   * @param string $sessionId
   *   The SugarCRM session Id.
   */
  abstract protected function logout($sessionId);

  /**
   * Fetches metadata about available links and fields
   *   associated with the desired SugarCRM module,
   *   for use in the Sugar Aggregate edit form autocomplete functionality.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * 
   * @return array|null
   *   Associative array of metadata about available links and fields,
   *   or null if something fails.
   */
  public function getMetadata($sugarAggregate) {
    $sessionId = $this->login($sugarAggregate);
    if (empty($sessionId)) {
      return NULL;
    }
    $availableLinks = array();
    $fieldMappings = $this->getFieldMappings($sugarAggregate, $sessionId, $availableLinks);
    $availableFields = array_map(function($fieldsInfo) { return array_keys($fieldsInfo); }, $fieldMappings);
    $this->logout($sessionId);
    return array(
      'availableLinks' => $availableLinks,
      'availableFields' => $availableFields,
    );
  }

  /**
   * Fetches full data associated with the passed Sugar Aggregate object,
   *   parses and aggregates it into page content ready to write to a node.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * 
   * @return string|null
   *   The fully parsed and prepared page content, or null if something fails.
   */
  public function import($sugarAggregate) {
    $sessionId = $this->login($sugarAggregate);
    if (empty($sessionId)) {
      return NULL;
    }
    $fieldMappings = $this->getFieldMappings($sugarAggregate, $sessionId);
    $entryList = $this->getEntryList($sugarAggregate, $sessionId);
    if (empty($entryList)) {
      return NULL;
    }
    $content = $this->parseContent($sugarAggregate, $fieldMappings, $entryList);
    $this->logout($sessionId);
    return $content;
  }

  /**
   * Returns the full SugarCRM API URL,
   *   based on the 'sugar_aggregate_api_url' persistent variable.
   *
   * @return string
   *   The URL.
   */
  protected function getUrl() {
    return variable_get('sugar_aggregate_api_url') . $this->getUrlSuffix();
  }

  /**
   * Logs the passed message stripped of any sensitive information.
   * 
   * @param string $message
   *   The log message to clean.
   */
  protected function cleanLog($message) {
    $clean = preg_replace('/"(password|session)"\s*:\s*"[^"]+"/', '"$1":"********"', $message);
    drupal_set_message($clean);
  }

  /**
   * Fetches metadata about links associated to the main module,
   *   and fields associated to the module and link,
   *   structures that data into an associative array and returns it.
   *   If $availableLinks is supplied, populates it with a list of
   *   links associated to the main module.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * @param string $sessionId
   *   The SugarCRM session Id.
   * @param string[] $availableLinks
   *   List of links associated with the main module.
   * 
   * @return array
   *   Associative array of metadata about module and link fields.
   */
  private function getFieldMappings($sugarAggregate, $sessionId, &$availableLinks = array()) {
    $sugarQuery = json_decode($sugarAggregate->sugar_query);
    $moduleName = $sugarQuery->module_name;
    $moduleFieldsInfo = array();
    if (!empty($moduleName)) {
      $moduleFieldsInfo = $this->getModuleFields($sugarAggregate, $sessionId, $moduleName);
      if (!empty($moduleFieldsInfo['link_fields'])) {
        $availableLinks = array_column($moduleFieldsInfo['link_fields'], 'name');
      }
    }
    $linkName = $sugarQuery->link_name;
    $linkFieldsInfo = array();
    if (!empty($linkName) && !empty($moduleFieldsInfo['link_fields'])) {
      $linkField = current(array_filter($moduleFieldsInfo['link_fields'], function($f) use($linkName) {
        return $f['name'] == $linkName;
      }));
      if (!empty($linkField['module'])) {
        $linkFieldsInfo = $this->getModuleFields($sugarAggregate, $sessionId, $linkField['module']);
      }
    }
    $fieldsInfoByModule = array(
      $moduleName => $moduleFieldsInfo,
      $linkName => $linkFieldsInfo,
    );

    $parsedInfo = array();
    foreach ($fieldsInfoByModule as $moduleName => $fieldsInfo) {
      foreach ($fieldsInfo['module_fields'] as $fieldInfo) {
        $fieldName = $fieldInfo['name'];
        $parsedInfo[$moduleName][$fieldName] = array();
        foreach ($fieldInfo['options'] as $optionInfo) {
          $optionName = $optionInfo['name'];
          $parsedInfo[$moduleName][$fieldName][$optionName] = $optionInfo['value'];
        }
      }
    }
    return $parsedInfo;
  }

  /**
   * Creates and returns request data to be used in
   *   the get_entry_list API call.
   *
   * @param string $sessionId
   *   The SugarCRM session Id.
   * @param string $sugarQueryJson
   *   JSON-encoded query from the database,
   *   which specifies what data to request from the SugarCRM API.
   * @param int $offset
   *   The offset for the paging of the returned results.
   * 
   * @return array
   *   Associative array representing the request data.
   */
  protected function createGetEntryListRequest($sessionId, $sugarQueryJson, $offset) {
    $sugarQuery = json_decode($sugarQueryJson);
    $moduleName = $sugarQuery->module_name;
    $moduleFields = preg_split('/[, \t\n]/', $sugarQuery->module_fields, -1, PREG_SPLIT_NO_EMPTY);
    $linkName = $sugarQuery->link_name;
    $linkFields = preg_split('/[, \t\n]/', $sugarQuery->link_fields, -1, PREG_SPLIT_NO_EMPTY);

    return array(
      'session' => $sessionId,
      'module_name' => $moduleName,
      'query' => '',
      'order_by' => '',
      'offset' => $offset,
      'select_fields' => $moduleFields,
      'link_name_to_fields_array' => array(
        array(
          'name' => $linkName,
          'value' => $linkFields,
        ),
      ),
      'max_results' => self::MAX_RESULTS,
      'deleted' => 0,
    );
  }

  /**
   * Parses the structured content returned by the get_entry_list call,
   *   with help from the metadata in $fieldMappings,
   *   into page content ready to write to a node.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * @param array $fieldMappings
   *   Associative array of metadata about module and link fields.
   * @param array $entryList
   *   Associative array of module and relationship entries.
   * 
   * @return string
   *   The fully parsed and prepared page content.
   */
  private function parseContent($sugarAggregate, $fieldMappings, $entryList) {
    $sugarQuery = json_decode($sugarAggregate->sugar_query);
    $moduleName = $sugarQuery->module_name;
    $linkName = $sugarQuery->link_name;
    $skipConditions = $this->parseConditions($sugarAggregate->skip_conditions);
    $placeholdersInLoop = $this->parsePlaceholders($sugarAggregate->loop_template);
    $moduleEntries = $entryList['module'];
    $relationshipEntries = $entryList['relationship'];

    $loopContent = '';
    foreach ($moduleEntries as $i => $entry) {
      $entryParsed = array();
      array_walk($entry['name_value_list'], function($field) use(&$entryParsed) {
        $fieldName = $field['name'];
        $entryParsed[$fieldName] = $field['value'];
      });
      $relationshipRecords = NULL;
      if (!empty($relationshipEntries[$i]['link_list'][0]['records'])) {
        $relationshipRecords = $relationshipEntries[$i]['link_list'][0]['records'];
      }
      else {
        $relationshipRecords = array(array('link_value' => array('' => array('value' => ''))));
      }
      foreach ($relationshipRecords as $j => $relationship) {
        $relationshipParsed = array();
        array_walk($relationship['link_value'], function($field) use(&$relationshipParsed) {
          $fieldName = $field['name'];
          $relationshipParsed[$fieldName] = $field['value'];
        });
        $modulesParsed = array(
          $moduleName => $entryParsed,
          $linkName => $relationshipParsed,
        );
        $loopReplacements = array();
        foreach ($placeholdersInLoop as $placeholderKey => $placeholder) {
          $moduleKey = $placeholder['moduleKey'];
          $fieldKey = $placeholder['fieldKey'];
          $value = isset($modulesParsed[$moduleKey][$fieldKey])
            ? $modulesParsed[$moduleKey][$fieldKey]
            : '';
          $mappedValue = isset($fieldMappings[$moduleKey][$fieldKey][$value])
            ? $fieldMappings[$moduleKey][$fieldKey][$value]
            : $value;
          if (isset($skipConditions[$placeholderKey])) {
            $comp = $skipConditions[$placeholderKey][0];
            $compVal = $skipConditions[$placeholderKey][1];
            $valueMatches = ($compVal == $value || $compVal == $mappedValue);
            if (($comp == self::COMP_EQ && $valueMatches) || ($comp == self::COMP_NEQ && !$valueMatches)) {
              $loopReplacements = array();
              break;
            }
          }
          $loopReplacements['{{' . $placeholderKey . '}}'] = $mappedValue;
        }
        $loopHtml = strtr($sugarAggregate->loop_template, $loopReplacements);
        $loopContent .= $loopHtml;
      }
    }
    $content = $sugarAggregate->content_template;
    $content = strtr($content, array(self::LOOP_PLACEHOLDER => $loopContent));
    return $content;
  }

  /**
   * Parses the skip conditions string into a structure to use
   *   when parsing the fetched content.
   *
   * @param string $conditionsString
   *   Skip conditions string as entered on the input form.
   * 
   * @return array
   *   Associative array of conditions as comparators and values.
   */
  private function parseConditions($conditionsString) {
    if (empty($conditionsString)) return array();

    $parsed = array();
    $conditionLines = explode("\n", $conditionsString);
    foreach ($conditionLines as $line) {
      foreach (array(self::COMP_EQ, self::COMP_NEQ) as $comparator) {
        if (strpos($line, $comparator) !== FALSE) {
          $split = explode($comparator, $line);
          $field = $split[0];
          $value = $split[1];
          $parsed[$field] = array($comparator, $value);
        }
      }
    }
    return $parsed;
  }

  /**
   * Parses the placeholders from a loop template into a two-dimensional array,
   *   with the placeholder as the key,
   *   and an associative array of the placeholder components as the value.
   *
   * @param string $loopTemplate
   *   Loop template as entered on the input form.
   * 
   * @return array
   *   Associative array of placeholder -> placeholder components array.
   */
  private function parsePlaceholders($loopTemplate) {
    $placeholdersInLoop = array();
    $placeholdersMatches = array();
    preg_match_all(self::PLACEHOLDER_REGEX, $loopTemplate, $placeholdersMatches, PREG_SET_ORDER);
    foreach ($placeholdersMatches as $placeholderMatch) {
      $placeholder = $placeholderMatch[1];
      $placeholdersInLoop[$placeholder] = array(
        'moduleKey' => $placeholderMatch[2],
        'fieldKey' => $placeholderMatch[3],
      );
    }
    return $placeholdersInLoop;
  }
}

/**
 * {@inheritDoc}
 */
class SugarCommunicatorRest extends SugarCommunicator {
  /**
   * What to use as the input and response content type for all requests.
   * 
   * @var string TYPE_JSON
   */
  const TYPE_JSON = 'JSON';

  /**
   * {@inheritDoc}
   */
  protected function getUrlSuffix() {
    return '/service/v4_1/rest.php';
  }

  /**
   * {@inheritDoc}
   */
  protected function login($sugarAggregate) {
    $curl = curl_init();
    $restData = array(
      'user_auth' => array(
        'user_name' => variable_get('sugar_aggregate_username'),
        'password' => variable_get('sugar_aggregate_password_hash'),
      )
    );
    $postFields = array(
      'method' => self::METHOD_LOGIN,
      'rest_data' => json_encode($restData),
      'input_type' => self::TYPE_JSON,
      'response_type' => self::TYPE_JSON,
    );
    curl_setopt_array($curl, array(
      CURLOPT_RETURNTRANSFER => 1,
      CURLOPT_URL => $this->getUrl(),
      CURLOPT_HTTPHEADER => array(
        'Content-Type: application/x-www-form-urlencoded'
      ),
      CURLOPT_POST => 1,
      CURLOPT_POSTFIELDS => http_build_query($postFields),
    ));
    $result = curl_exec($curl);
    $decodedResult = json_decode($result, TRUE);
    if (!$result || !$decodedResult || !$decodedResult['id']) {
      $this->cleanLog('login - bad result:<br/>' . $result . '<br/>request data:<br/>' . json_encode($restData));
      curl_close($curl);
      return NULL;
    }
    curl_close($curl);
    return $decodedResult['id'];
  }

  /**
   * {@inheritDoc}
   */
  protected function getModuleFields($sugarAggregate, $sessionId, $moduleName) {
    $curl = curl_init();
    $restData = array(
      'session' => $sessionId,
      'module_name' => $moduleName,
      'md5' => FALSE,
    );
    $postFields = array(
      'method' => self::METHOD_GET_MODULE_FIELDS,
      'rest_data' => json_encode($restData),
      'input_type' => self::TYPE_JSON,
      'response_type' => self::TYPE_JSON,
    );
    curl_setopt_array($curl, array(
      CURLOPT_RETURNTRANSFER => 1,
      CURLOPT_URL => $this->getUrl(),
      CURLOPT_HTTPHEADER => array(
        'Content-Type: application/x-www-form-urlencoded'
      ),
      CURLOPT_POST => 1,
      CURLOPT_POSTFIELDS => http_build_query($postFields),
    ));
    $result = curl_exec($curl);
    $decodedResult = json_decode($result, TRUE);
    if (!$result || !$decodedResult || !$decodedResult['module_name']) {
      $this->cleanLog('get_module_fields - bad result:<br/>' . $result . '<br/>request data:<br/>' . json_encode($restData));
      curl_close($curl);
      return array();
    }
    curl_close($curl);
    return $decodedResult;
  }

  /**
   * {@inheritDoc}
   */
  protected function getEntryList($sugarAggregate, $sessionId) {
    $curl = curl_init();
    curl_setopt_array($curl, array(
      CURLOPT_RETURNTRANSFER => 1,
      CURLOPT_URL => $this->getUrl(),
      CURLOPT_HTTPHEADER => array(
        'Content-Type: application/x-www-form-urlencoded'
      ),
      CURLOPT_POST => 1,
    ));
    $restData = $this->createGetEntryListRequest($sessionId, $sugarAggregate->sugar_query, 0);

    $moduleEntries = array();
    $relationshipEntries = array();
    $totalCount = INF;
    for ($offset = 0; $offset < $totalCount; $offset += self::MAX_RESULTS) {
      $restData['offset'] = $offset;
      $postFields = array(
        'method' => self::METHOD_GET_ENTRY_LIST,
        'rest_data' => json_encode($restData),
        'input_type' => self::TYPE_JSON,
        'response_type' => self::TYPE_JSON,
      );
      curl_setopt_array($curl, array(
        CURLOPT_POSTFIELDS => http_build_query($postFields),
      ));
      $result = curl_exec($curl);
      $decodedResult = json_decode($result, TRUE);
      if (!$result || !$decodedResult || !$decodedResult['result_count']) {
        $this->cleanLog('get_entry_list - bad result:<br/>' . $result . '<br/>request data:<br/>' . json_encode($restData));
        curl_close($curl);
        return array();
      }
  
      $totalCount = $decodedResult['total_count'];
      $moduleEntries = array_merge($moduleEntries, $decodedResult['entry_list']);
      $relationshipEntries = array_merge($relationshipEntries, $decodedResult['relationship_list']);
    }
    curl_close($curl);
    return array(
      'module' => $moduleEntries,
      'relationship' => $relationshipEntries,
    );
  }

  /**
   * {@inheritDoc}
   */
  protected function logout($sessionId) {
    $curl = curl_init();
    $restData = array(
      'session' => $sessionId,
    );
    $postFields = array(
      'method' => self::METHOD_LOGOUT,
      'rest_data' => json_encode($restData),
      'input_type' => self::TYPE_JSON,
      'response_type' => self::TYPE_JSON,
    );
    curl_setopt_array($curl, array(
      CURLOPT_RETURNTRANSFER => 1,
      CURLOPT_URL => $this->getUrl(),
      CURLOPT_HTTPHEADER => array(
        'Content-Type: application/x-www-form-urlencoded'
      ),
      CURLOPT_POST => 1,
      CURLOPT_POSTFIELDS => http_build_query($postFields),
    ));
    curl_exec($curl);
    curl_close($curl);
  }
}

/**
 * {@inheritDoc}
 */
class SugarCommunicatorCustomSoap extends SugarCommunicator {
  /**
   * If necessary loads the SOAP client module,
   *   instantiates a SOAP client object, initializes, and returns it.
   *
   * @return \nusoap_client
   *   The SOAP client object.
   */
  private function getClient() {
    $nusoapLoaded = module_load_include('php', 'drupaltosugar', 'nusoap/lib/nusoap');
    if (!$nusoapLoaded) {
      module_load_include('php', 'sugar_aggregate', 'nusoap/lib/nusoap');
    }
    $client = new nusoap_client($this->getUrl());
    $client->soap_defencoding = 'UTF-8';
    $client->decode_utf8 = FALSE;
    return $client;
  }

  /**
   * {@inheritDoc}
   */
  protected function getUrlSuffix() {
    return '/customsoap.php?wsdl';
  }

  /**
   * {@inheritDoc}
   */
  protected function login($sugarAggregate) {
    $client = $this->getClient();
    $soapData = array(
      'user_auth' => array(
        'user_name' => variable_get('sugar_aggregate_username'),
        'password' => variable_get('sugar_aggregate_password_hash'),
      )
    );
    $result = $client->call(self::METHOD_LOGIN, $soapData);
    if (!$result || !$result['id']) {
      $this->cleanLog('login - bad result:<br/>' . json_encode($result) . '<br/>request data:<br/>' . json_encode($soapData));
      return NULL;
    }
    return $result['id'];
  }

  /**
   * {@inheritDoc}
   */
  protected function getModuleFields($sugarAggregate, $sessionId, $moduleName) {
    $client = $this->getClient();
    $soapData = array(
      'session' => $sessionId,
      'module_name' => $moduleName,
      'md5' => FALSE,
    );
    $result = $client->call(self::METHOD_GET_MODULE_FIELDS, $soapData);
    if (!$result || !$result['module_name']) {
      $this->cleanLog('get_module_fields - bad result:<br/>' . json_encode($result) . '<br/>request data:<br/>' . json_encode($soapData));
      return array();
    }
    return $result;
  }

  /**
   * {@inheritDoc}
   */
  protected function getEntryList($sugarAggregate, $sessionId) {
    $client = $this->getClient();
    $soapData = $this->createGetEntryListRequest($sessionId, $sugarAggregate->sugar_query, 0);

    $moduleEntries = array();
    $relationshipEntries = array();
    $totalCount = INF;
    for ($offset = 0; $offset < $totalCount; $offset += self::MAX_RESULTS) {
      $soapData['offset'] = $offset;
      $result = $client->call(self::METHOD_GET_ENTRY_LIST, $soapData);
      if (!$result || !$result['result_count']) {
        $this->cleanLog('get_entry_list - bad result:<br/>' . json_encode($result) . '<br/>request data:<br/>' . json_encode($soapData));
        return array();
      }

      $totalCount = $result['total_count'];
      $moduleEntries = array_merge($moduleEntries, $result['entry_list']);
      $relationshipEntries = array_merge($relationshipEntries, $result['relationship_list']);
    }
    return array(
      'module' => $moduleEntries,
      'relationship' => $relationshipEntries,
    );
  }

  /**
   * {@inheritDoc}
   */
  protected function logout($sessionId) {
    $client = $this->getClient();
    $soapData = array(
      'session' => $sessionId,
    );
    $client->call(self::METHOD_LOGOUT, $soapData);
  }
}
