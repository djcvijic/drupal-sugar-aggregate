<?php

/**
 * @file
 * Contains SugarAggregator class definition.
 */

/**
 * Saves the content fetched and parsed from SugarCRM to a node.
 */
class SugarAggregator {
  /**
   * The entity type to use when creating/loading an entity.
   * 
   * @var string ENTITY_TYPE
   */
  const ENTITY_TYPE = 'node';

  /**
   * The format to use for the body of the node.
   * 
   * @var string BODY_FORMAT
   */
  const BODY_FORMAT = 'full_html';

  /**
   * The pattern to match when inserting the imported time
   *   into the body template.
   * 
   * @var string TIME_PLACEHOLDER
   */
  const TIME_PLACEHOLDER = '{{time}}';

  /**
   * The revision operation to use when creating/loading an entity,
   *   if the Revisioning module is enabled.
   * 
   * @var int REVISION_OPERATION
   */
  const REVISION_OPERATION = REVISIONING_NEW_REVISION_NO_MODERATION;

  /**
   * The revision condition to use when creating/loading an entity,
   *   if the Revisioning module is enabled.
   * 
   * @var int REVISION_CONDITION
   */
  const REVISION_CONDITION = REVISIONING_NEW_REVISION_WHEN_NOT_PENDING;

  /**
   * Processes the content fetched and parsed from SugarCRM,
   *   saves it to a new or pre-existing node,
   *   creates or modified the path to the node,
   *   and updates the Sugar Aggregate info in the database.
   *
   * @param object $sugarAggregate
   *   The Sugar Aggregate object from the database.
   * @param string $contentBody
   *   The text content fetched and parsed from SugarCRM.
   * @param boolean $force
   *   Whether to re-process even if the new content is the same as before.
   * 
   * @return int
   *   The node Id of the associated node.
   */
  public function process($sugarAggregate, $contentBody, $force = FALSE) {
    // Check if this item already exists.
    // Load an existing entity.
    $sugarAggregateId = $sugarAggregate->id;
    $nodeId = $sugarAggregate->node_nid;
    $nodeType = $sugarAggregate->node_type;
    $nodeLanguage = $sugarAggregate->node_language;
    $nodeAuthorUid = $sugarAggregate->node_author_uid;
    $entity = $this->entityLoad($nodeId, $nodeLanguage, $nodeAuthorUid);

    $content = array(
      'title' => $sugarAggregate->content_title,
      'summary' => $sugarAggregate->content_summary,
      'body' => $contentBody,
      'type' => $nodeType,
      'language' => $nodeLanguage,
      'author' => $nodeAuthorUid,
      'pathAlias' => $sugarAggregate->node_path_alias,
    );
    $newHash = md5(serialize($content));

    if (empty($entity)) {
      // Build a new entity.
      $entity = $this->newEntity($nodeType, $nodeLanguage, $nodeAuthorUid);
    }
    elseif (!$force && $newHash === $sugarAggregate->content_hash) {
      // Do not proceed if the item exists, has not changed,
      // and we're not forcing the update.
      return $entity->nid;
    }

    $this->populateEntity($entity, $content);

    // This will throw an exception on failure.
    $this->entitySave($entity);

    // Save new fields to sugar_aggregate
    $count = db_update('sugar_aggregate')
      ->fields(array(
        'content_hash' => $newHash,
        'node_nid' => $entity->nid,
        'imported' => REQUEST_TIME,
      ))
      ->condition('id', $sugarAggregateId, '=')
      ->execute();

    return $entity->nid;
  }

  /**
   * Creates a new node object and prepares it to be saved.
   * 
   * @param string $nodeType
   *   Node type.
   * @param string $nodeLanguage
   *   Node language.
   * @param int $nodeAuthorUid
   *   User Id of the node author.
   * 
   * @return object
   *   New node object.
   */
  private function newEntity($nodeType, $nodeLanguage, $nodeAuthorUid) {
    $node = new stdClass();

    $info = entity_get_info(self::ENTITY_TYPE);
    if (!empty($info['entity keys']['language'])) {
      $node->{$info['entity keys']['language']} = $nodeLanguage;
    }

    $node->type = $nodeType;
    node_object_prepare($node);

    // Populate properties that are set by node_object_prepare().
    $node->status = TRUE;
    $node->promote = FALSE;
    $node->comment = TRUE;
    $node->revision_operation = self::REVISION_OPERATION;
    $node->revision_condition = self::REVISION_CONDITION;
    $node->uid = $nodeAuthorUid;
    $node->log = 'Created by SugarAggregate';

    return node_submit($node);
  }

  /**
   * Fetches an existing node object from the database
   *   and prepares it to be saved again.
   *
   * @param int $nodeId
   *   Id of the node.
   * @param string $nodeLanguage
   *   Node language.
   * @param int $nodeAuthorUid
   *   User Id of the node author.
   * 
   * @return object
   *   New node object.
   */
  private function entityLoad($nodeId, $nodeLanguage, $nodeAuthorUid) {
    if (empty($nodeId)) return NULL;

    $entities = entity_load(self::ENTITY_TYPE, array($nodeId));
    if (empty($entities)) return NULL;

    $node = reset($entities);

    $info = entity_get_info(self::ENTITY_TYPE);
    if ($node && !empty($info['entity keys']['language'])) {
      $node->{$info['entity keys']['language']} = $nodeLanguage;
    }

    node_object_prepare($node);

    // Populate properties that are set by node_object_prepare().
    $node->revision_operation = self::REVISION_OPERATION;
    $node->revision_condition = self::REVISION_CONDITION;
    $node->uid = $nodeAuthorUid;
    $node->log = 'Updated by SugarAggregate';

    // Workaround for issue #1247506. See #1245094 for backstory.
    if (!empty($node->menu)) {
      // If the node has a menu item(with a valid mlid)
      // it must be flagged 'enabled'.
      $node->menu['enabled'] = (int)(bool)$node->menu['mlid'];
    }

    return $node;
  }

  /**
   * Populates the node object with the desired content and metadata.
   *
   * @param object $entity
   *   Node object to be modified.
   * @param array $content
   *   Associative array with the different parts of node content.
   */
  private function populateEntity(&$entity, $content) {
    $contentBody = str_replace(self::TIME_PLACEHOLDER, date('Y-m-d H:i:s'), $content['body']);
    $nodeLanguage = $content['language'];
    $nodePathAlias = $content['pathAlias'];
    $entity->title = $content['title'];
    $entity->body[$nodeLanguage][0] = array(
      'value' => $contentBody,
      'summary' => $content['summary'],
      'format' => self::BODY_FORMAT,
    );
    if (!empty($nodePathAlias)) {
      if (!isset($entity->path)) {
        $entity->path = array();
      }
      $entity->path['pathauto'] = FALSE;
      $entity->path['alias'] = $nodePathAlias;
    }
  }

  /**
   * Checks that the desired user is authenticated to create/modify the node,
   *   and then saves the node.
   *
   * @param object $entity
   *   The node object.
   * 
   * @throws \Exception
   *   If authentication fails.
   */
  private function entitySave($entity) {
    // The check will be skipped for anonymous nodes.
    if (!empty($entity->uid)) {

      $author = user_load($entity->uid);

      // If the uid was mapped directly, rather than by email or username,
      // it could be invalid.
      if (!$author) {
        $message = t('User %uid is not a valid user.', array(
          '%uid' => $entity->uid
        ));
        throw new \Exception($message);
      }

      if (empty($entity->nid) || !empty($entity->is_new)) {
        $op = 'create';
        $access = node_access($op, $entity->type, $author);
      }
      else {
        $op = 'update';
        $access = node_access($op, $entity, $author);
      }

      if (!$access) {
        $message = t('The user %name is not authorized to %op content of type %content_type. To import this item.', array(
          '%name' => $author->name,
          '%op' => $op,
          '%content_type' => $entity->type,
          '@name' => $author->name,
          '@op' => $op,
          '@content_type' => $entity->type,
        ));
        throw new \Exception($message);
      }
    }

    node_save($entity);
  }
}
