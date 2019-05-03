(function($) {
  Drupal.behaviors.sugar_aggregate = {
    attach: function (context, settings) {
      var $form = $('.sugar-aggregate-form');
      var $moduleNameInput = $form.find('.module-name-input');
      var $linkNameInput = $form.find('.link-name-input');
      var $moduleFieldsInput = $form.find('.module-fields-input');
      var $linkFieldsInput = $form.find('.link-fields-input');
      var sugarMetadata = $form.data('sugar-metadata');

      var getModuleName = function() {
        return $moduleNameInput.val();
      };
      var getLinkName = function() {
        return $linkNameInput.val();
      };
      var getAvailableFieldsModule = function() {
        var availableFields = sugarMetadata && sugarMetadata.availableFields[getModuleName()] || [];
        return availableFields.map(i => { return { value: i } });
      };
      var getAvailableFieldsLink = function() {
        var availableFields = sugarMetadata && sugarMetadata.availableFields[getLinkName()] || [];
        return availableFields.map(i => { return { value: i } });
      };

      var initAutoCompleteEngine = function($fieldsInput, availableItems) {
        var engine = new Bloodhound({
          limit: 10,
          local: availableItems,
          datumTokenizer: function(str) {
            return str && str.value && str.value.split(/[_\W]+/) || [];
          },
          queryTokenizer: function(str) {
            return str && str.split(/[_\W]+/) || [];
          }
        });
        engine.initialize();
        $fieldsInput.data('autoCompleteEngine', engine);
        return engine;
      };

      if (sugarMetadata && sugarMetadata.availableLinks.length) {
        var availableLinks = sugarMetadata.availableLinks.map(i => { return { value: i } });
        $linkNameInput.typeahead({
          hint: true,
          highlight: true,
          minLength: 1
        }, {
          displayKey: 'value',
          source: initAutoCompleteEngine($linkNameInput, availableLinks).ttAdapter()
        });
      }

      var updateSuggests = function(firstTime) {
        var modulePrefix = getModuleName() + '.';
        var linkPrefix = getLinkName() + '.';
        var moduleSuggests = $moduleFieldsInput
          .val()
          .split(',')
          .map(function(f) {
            return modulePrefix + f;
          });
        var linkSuggests = $linkFieldsInput
          .val()
          .split(',')
          .map(function(f) {
            return linkPrefix + f;
          });
        var suggests = moduleSuggests.concat(linkSuggests);

        $form.find('.skip-conditions-input').asuggest(
          [modulePrefix, linkPrefix].concat(suggests),
          {
            endingSymbols: '',
            delimiters: '\n',
            minChunkSize: 0,
            stopSuggestionKeys: [$.asuggestKeys.RETURN],
            cycleOnTab: true
          }
        );

        $form.find('.content-template-input').asuggest(
          ['{{loop}}', '{{time}}'],
          {
            endingSymbols: '',
            delimiters: ' \t\n>}',
            minChunkSize: 2,
            stopSuggestionKeys: [$.asuggestKeys.RETURN],
            cycleOnTab: true
          }
        );

        $form.find('.loop-template-input').asuggest(
          ['{{' + modulePrefix, '{{' + linkPrefix]
            .concat(suggests
            .map(function(s) {
              return '{{' + s + '}}';
            })),
          {
            endingSymbols: '',
            delimiters: ' \t\n>}',
            minChunkSize: 2,
            stopSuggestionKeys: [$.asuggestKeys.RETURN],
            cycleOnTab: true
          }
        );

        if (firstTime !== true) {
          var moduleFieldsEngine = $moduleFieldsInput.data('autoCompleteEngine');
          moduleFieldsEngine.clear();
          moduleFieldsEngine.local = getAvailableFieldsModule();
          moduleFieldsEngine.initialize(true);

          var linkFieldsEngine = $linkFieldsInput.data('autoCompleteEngine');
          linkFieldsEngine.clear();
          linkFieldsEngine.local = getAvailableFieldsLink();
          linkFieldsEngine.initialize(true);
        }
      };

      $moduleFieldsInput.tokenfield({
        delimiter: ',',
        beautify: false,
        typeahead: [null, {
          displayKey: 'value',
          source: initAutoCompleteEngine($moduleFieldsInput, getAvailableFieldsModule()).ttAdapter()
        }]
      });

      $linkFieldsInput.tokenfield({
        delimiter: ',',
        beautify: false,
        typeahead: [null, {
          displayKey: 'value',
          source: initAutoCompleteEngine($linkFieldsInput, getAvailableFieldsLink()).ttAdapter()
        }]
      });

      var validateAddField = function(event) {
        var $fieldsInput = $(event.target);
        var newField = event.attrs.value;
        var autoCompleteEngine = $fieldsInput.data('autoCompleteEngine');
        var availableFields = autoCompleteEngine && autoCompleteEngine.local;
        var invalid = availableFields && availableFields.length && !availableFields.some(f => f.value === newField);
        if (invalid) {
          event.preventDefault();
          return;
        }

        var existingFields = $fieldsInput.tokenfield('getTokens');
        var duplicate = existingFields && existingFields.some(f => f.value === newField);
        if (duplicate) {
          event.preventDefault();
        }
      };

      updateSuggests(true);

      $form.find('.module-name-input, .link-name-input')
        .on('change', updateSuggests);

      $form.find('.module-fields-input, .link-fields-input')
        .on('tokenfield:createtoken', validateAddField)
        .on('change', updateSuggests);

      var $cronEnabled = $form.find('.cron-enabled-input');
      var $cronPeriodUnit = $form.find('.cron-period-unit-input');
      var $cronPeriodAmount = $form.find('.cron-period-amount-input');

      $cronPeriodAmount[0].type = 'number';

      var updateCron = function() {
        var disabled = !$cronEnabled.prop('checked');
        $cronPeriodUnit.attr('disabled', disabled);
        $cronPeriodAmount.attr('disabled', disabled);
      };

      updateCron();

      $cronEnabled.on('click', updateCron);
    },
  };
})(jQuery);
