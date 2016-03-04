/* 
 * Process for meetings. Set dates 
 * jrobens@interlated.com.au
 * 2013003
 * 
 */

(function($) {
    Drupal.behaviors.MeetingProcess = {
        attach: function(context, settings) {
            $('#edit-field-meeting-date-und-0-value-datepicker-popup-0', context).change(function() {
                // 21 Mar 2013 format
                var meetingDate = $(this).val();
                var cutOffDate = new Date(meetingDate);
                cutOffDate.setDate(cutOffDate.getDate()-2);
                
                var m_names = new Array("Jan", "Feb", "Mar", 
                    "Apr", "May", "Jun", "Jul", "Aug", "Sep", 
                    "Oct", "Nov", "Dec");
                var month = m_names[cutOffDate.getMonth()];
                var formattedMonth = cutOffDate.getDate() + ' ' + month + ' ' + cutOffDate.getFullYear();
 
                
                $('#edit-field-submission-cutoff-und-0-value-datepicker-popup-0', context).val(formattedMonth);
            });
        }
    }
})(jQuery);


