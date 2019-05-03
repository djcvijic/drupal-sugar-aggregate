SUGAR AGGREGATE
===============


CONTENTS OF THIS FILE
---------------------

 * Introduction
 * Requirements
 * Recommended Modules
 * Installation
 * Configuration and Usage
 * Troubleshooting
 * Maintainers


INTRODUCTION
------------

Sugar Aggregate connects to a SugarCRM instance, pulls down data about
all entries belonging to a specific SugarCRM module, and up to one
related (linked) SugarCRM module, aggregates that data into the
specified format, and saves it to a single node on the Drupal instance.

Features:

 * Connect to SugarCRM via Rest 4.1 or Custom Soap API.
 * An arbitrary number of Sugar Aggregates can be defined,
   to import to multiple nodes.
 * Form auto-completion for easier entry (disable-able).
 * Conditional exclusion of imported entries.
 * Automated creation of new node and an associated custom path.
 * On-demand and automated periodic importing of data.

The module's project page:
https://www.drupal.org/project/sugar_aggregate

To view full documentation, track changes, or contribute to development:
https://github.com/djcvijic/drupal-sugar-aggregate#readme

To submit bug reports and feature suggestions:
https://github.com/djcvijic/drupal-sugar-aggregate/issues


REQUIREMENTS
------------

No special requirements.


RECOMMENDED MODULES
-------------------

 * Markdown filter (https://www.drupal.org/project/markdown):

   When enabled, display of the project's README.md help will be rendered
   with markdown.


INSTALLATION
------------

 * Install as you would normally install a contributed Drupal module. Visit:
   https://www.drupal.org/documentation/install/modules-themes/modules-7
   for further information.


CONFIGURATION AND USAGE
-----------------------

 * Configure user permissions in Administration >> People >> Permissions:

   - Use the administration pages and help (System module)

     The top-level administration categories require this permission to be
     accessible. The administration menu will be empty unles this permission
     is granted.

   - Access administration menu / Use the administration toolbar

     Users in roles with this permission will see the administration menu at
     the top of each page.

   - Administer Sugar Aggregate

     Users in roles with this permission will be able to access all views and
     execute all actions available in the Sugar Aggregate module.

 * Configure global Sugar Aggregate settings in Administration >>
   Sugar Aggregate >> Configure Sugar Aggregate

   - Sugar API URL

     Full URL of the SugarCRM instance (e.g. http://www.mydomain.com/sugarcrm).

   - Sugar Username

     This user must have access to authenticate via the API and read
     module entries.

   - Sugar Password

     The password associated with the given user.

   - Sugar API Protocol

     Protocol to use to communicate with SugarCRM, CustomSoap may offer
     a smoother integration than Rest but may not be available.

   - Disable Javascript

     If there is a problem with the javascript on the Sugar Aggregate
     add/edit pages, this is the kill switch.

 * Check main configuration page at Administration >> Sugar Aggregate

   - Configure Sugar Aggregate

     Leads to the global configuration page.

   - Table of Sugar Aggregates

     Each row contains info about a single configuration that imports data to
     a single node, and actions that can be done on that configuration.

   - View imported content

     Visible if the Sugar Aggregate has ever been imported.
     Links to the node with the imported content.

   - Import

     Action to manually run an import immediately.
     Skips checking whether the content has changed and always executes import.

   - Edit

     Opens the form to edit the Sugar Aggregate.
     Any changes will be applied on next import.

   - Delete

     Delete the Sugar Aggregate.

   - Add new record

     Opens the form to add a new Sugar Aggregate.

 * Add/edit Sugar Aggregate

   - Sugar Module Name

     The base module for which information is being fetched (e.g. Leads).

   - Sugar Module Fields

     A comma-separated list of fields to fetch for the module above
     (e.g. id,last_name,phone_work).

   - Sugar Link Name

     An extra module linked to the module above (e.g. email_addresses).

   - Sugar Link Fields

     A comma-separated list of fields to fetch for the linked module
     (e.g. id,primary_address,opt_out).

   - Skip Conditions

     One condition per line,
     in the form module.field==123 (comparators: == and !=),
     if ANY condition is met for an item,
     it is skipped (e.g. email_addresses.opt_out==1).

   - Content Title

     Title for the imported content.

   - Content Summary

     Summary for the imported content.

   - Content Template

     Main template for the imported content,
     use {{loop}} to mark where looped items should be injected,
     {{time}} to print the time of import.

   - Loop Template

     Template which will be rendered once for each fetched item,
     use {{MODULE.FIELD}} to print their values (e.g. {{Leads.last_name}}).

   - Node Type

     Node type for the imported content,
     page is tested and recommended.

   - Node Language

     Language for the associated node,
     Language neutral is tested and recommended.

   - Node Author

     Associated node will be created/edited as this user,
     authentication will be done so they must have permissions
     to create/edit this node.

   - Node Path Alias

     Path alias to use for the URL of the attached node (e.g. "foo/bar"),
     by default Content Title is used.

   - Automatic Import

     Toggle and configure automatic periodic import.


TROUBLESHOOTING
---------------

 * Sugar Aggregate does not appear in the administration menu,
   or any of the Sugar Aggregate pages are inaccessible, or access denied:

   - Check that Sugar Aggregate is installed, that the module is enabled,
     and that permissions are set up for you to access the module.

   - Alternatively, try re-installing the module and trying again.

 * The Sugar Aggregate main page does not display a table,
   or the option to create new items:

   - Global settings must be configured before creating import configurations.
     Configure global Sugar Aggregate settings in Administration >>
     Sugar Aggregate >> Configure Sugar Aggregate.
     Make sure all text fields are filled, and form is submitted correctly.

 * Edit or Import print a message saying "bad request", or Import fetches
   no data or incomplete data:

   - Check that the Sugar API URL is correct.

   - Check that the SugarCRM instance is accessible from the
     Drupal instance (not behind a firewall, or in an inaccessible network).

   - Check that the Sugar Username and Sugar Password are correct and
     correspond to an actual user on the SugarCRM instance.

   - Check that the SugarCRM user has permission to read the module
     and entries being requested.

   - Alternatively, change the Sugar API Protocol and try again.

 * Edit form is buggy, or is making it hard to enter information,
   or is not saving information correctly on submit:

   - In the global Sugar Aggregate configuration settings,
     check Disable Javascript, submit the form, and try again.

 * Node is created and saved with correct information,
   but the custom path alias is not working:

   - After import, add a custom path alias manually.
     Go to the imported node, edit the node, go to URL path settings,
     enter a URL alias, and save the node.

 * Automatic import is set up, but it is never triggering:

   - Go to the main Sugar Aggregate configuration page.
     Check LAST IMPORTED to make sure that import didn't happen.
     Check that AUTOMATIC IMPORT shows the correct period.

   - Go to Administration >> Configuration >> System >> Cron.
     Make sure that cron is set up to run often enough.
     "Run cron every" should be set to a value lower than
     the desired automatic import period.
     Try clicking "Run cron" and check that this triggered the import.

   - Be aware that periodic import will be skipped when the source data
     has not changed since last time. If you need to re-import the same data,
     use the Import button.


MAINTAINERS
-----------

 * Djordje Cvijic (djcvijic) - https://www.drupal.org/user/3613460

