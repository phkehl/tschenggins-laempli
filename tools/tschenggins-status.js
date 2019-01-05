$(document).ready(function ()
{
    DEBUG('hello');

    // arm "configure" links in client list
    $('.action-configure-client').on('click', function (e)
    {
        var clientId = $(this).data('clientid');
        $('#tab-config').trigger('click');
        $('#tab-config-' + clientId).trigger('click');
    });

    // handle location hash and tabs
    $('input.tab-input').on('click', function (e)
    {
        var input = $(this);
        var id = input.attr('id');
        document.location.hash = id.replace(/^tab-/, '');
        DEBUG('select tab ' + id);
        if (id === 'tab-config')
        {
            if (input.data('last-config-tab'))
            {
                $('#' + input.data('last-config-tab')).trigger('click');
            }
            else
            {
                $('input[name=tabs-config]').prop('checked', false);
            }
        }
        else if (id.substr(0, 11) === 'tab-config-')
        {
            $('#tab-config').data('last-config-tab', id);
        }
    });
    {
        var id = document.location.hash.substr(1);
        if (id.length && $('#tab-' + id).length)
        {
            DEBUG('initial tab ' + id);
            if (id.substr(0, 7) === 'config-')
            {
                $('#tab-config').trigger('click');
            }
            $('#tab-' + id).trigger('click');
        }
        else
        {
            DEBUG('initial tab <default>');
            var firstTabId = $('input.tab-input').get(0).id;
            $('#' + firstTabId).trigger('click');
        }
    }

    // arm table sorts
    $('th.sort').on('click', function (e)
    {
        var col = $(this);
        var colIx = col.index();
        var table = col.parents('table').eq(0);
        var rows = table.find('tbody tr').toArray().sort(function (a, b)
        {
            var tdA = $(a).children('td').eq(colIx);
            var tdB = $(b).children('td').eq(colIx);
            var vA = tdA.data('sort') || tdA.text();
            var vB = tdB.data('sort') || tdB.text();
            return $.isNumeric(vA) && $.isNumeric(vB) ? vA - vB : vA.toString().localeCompare(vB);
        });
        col.data('asc', !col.data('asc'));
        if (!col.data('asc'))
        {
            rows = rows.reverse();
        }
        col.parent('tr').find('th').removeClass('sort-asc').removeClass('sort-desc');
        col.addClass(col.data('asc') ? 'sort-asc' : 'sort-desc');
        var tbody = table.find('tbody');
        tbody.append(rows);
    });

    // arm "modify" links in results list
    $('.action-modify-job').on('click', function (e)
    {
        var jobId = $(this).data('jobid');
        var input;
        if ($(this).data('server') === 'multijob')
        {
            $('#tab-multi').trigger('click');
            input = $("#modify-multi-form input[name='job'][value='" + jobId + "']").trigger('click');
        }
        else
        {
            $('#tab-modify').trigger('click');
            input = $("#modify-job-form input[name='job'][value='" + jobId + "']").trigger('click');
        }
        input.parents('.joblist').scrollTo(input, { offset: -50, duration: 300 });
    });

    // arm "delete" links in results list
    $('.action-delete-job').on('click', function (e)
    {
        var jobId = $(this).data('jobid');
        $('#tab-delete').trigger('click');
        var input = $("#delete-job-form input[name='job'][value='" + jobId + "']").prop('checked', true);
        input.parents('.joblist').scrollTo(input, { offset: -50, duration: 300 });
    });

    // block ui when sending form
    $('form').on('submit', function ()
    {
        $('body').prepend( $('<div/>').addClass('blocker'),
                           //$('<div/>').addClass('blocker-message').html('&hellip;please wait&hellip;'),
                           '<div class="blocker-message bouncing-loader"><div></div><div></div><div></div></div>');
        return true;
    });

    // results table filter
    var resultsFilter = $('#results-filter');
    var resultsStatus = $('#results-filter-status');
    var resultsTableRows = $('#results-table tbody tr');
    if (resultsFilter.length && resultsStatus.length && resultsTableRows.length)
    {
        if (sessionStorage.getItem('results-filter-term'))
        {
            resultsFilter.val( sessionStorage.getItem('results-filter-term') );
        }

        //DEBUG('resultsFilter', resultsFilter);
        //DEBUG('resultsTableRows', resultsTableRows);
        var index = resultsTableRows.map(function (ix, el)
        {
            return { tr: $(el), text: $(el).text() };
        });
        var filterTo;
        resultsFilter.on('keyup', function (e)
        {
            if (filterTo)
            {
                clearTimeout(filterTo);
            }
            filterTo = setTimeout(function () { filterTable(resultsFilter, index); }, 300);
        }).trigger('keyup');
    }
    function filterTable(input, index)
    {
        DEBUG('filter ' + input.val());
        var term = input.val();
        if (term.length == 0)
        {
            input.removeClass('error');
            index.each(function () { this.tr.removeClass('hidden'); });
            resultsStatus.removeClass('error');
            resultsStatus.text('showing all ' + index.length + ' jobs');
            sessionStorage.removeItem('results-filter-term');
            return;
        }
        var re;
        try
        {
            re = new RegExp(term, term.toLowerCase() == term ? 'i' : '');
        }
        catch (e)
        {
            DEBUG('bad regexp: ' + e);
            input.addClass('error');
            resultsStatus.text('bad regexp: ' + e);
            resultsStatus.addClass('error');
        }
        if (typeof re === 'object')
        {
            sessionStorage.setItem('results-filter-term', term);
            var nShow = 0;
            input.removeClass('error');
            for (var ix = 0; ix < index.length; ix++)
            {
                if (re.test( index[ix].text ))
                {
                    index[ix].tr.removeClass('hidden')
                    nShow++;
                }
                else
                {
                    index[ix].tr.addClass('hidden')
                }
            }
            resultsStatus.removeClass('error');
            resultsStatus.text('showing ' + nShow + ' of ' + index.length + ' jobs');

        }
    }

    // job modify form magic
    $('#modify-job-form input[name=job]').on('change', function (e)
    {
        var label = $(this).parent('label');
        var data = label.data();
        if (data.state)
        {
            $("#modify-job-form input[name='state'][value='" + data.state + "'").trigger('click');
        }
        if (data.result)
        {
            $("#modify-job-form input[name='result'][value='" + data.result + "'").trigger('click');
        }
    });

    // multijob modify form magic
    $('#modify-multi-form input[name=job]').on('change click', function (e)
    {
        var label = $(this).parent('label');
        var data = label.data();
        $("#modify-multi-form input[name='jobs']").prop('checked', false);
        data.multi.split(',').forEach(function (id)
        {
            $("#modify-multi-form input[name='jobs'][value='" + id + "'").prop('checked', true);
        });
        var firstInput = $("#modify-multi-form input[name='jobs']:checked:first");
        if (firstInput.length)
        {
            firstInput.parents('.joblist').scrollTo(firstInput, { offset: -50, duration: 300 });
        }
    });

    // multi-job info toggle
    $('.multijob-info-toggle').on('click', function (e)
    {
        $('#' + $(this).data('id')).toggle();
    });

    // form label highlighting
    $('form').on('change', 'input[type=radio]', function (e)
    {
        var label = $(this).parent('label');
        label.addClass('highlight').siblings().removeClass('highlight');
    });
    //$('form').on('change', 'input[type=checkbox]', function (e)
    //{
    //    var label = $(this).parent('label');
    //    label.addClass('highlight');
    //    $(this).siblings(':checked').parents('label').addClass('highlight');
    //    $(this).siblings().not(':checked').parents('label').removeClass('highlight');
    //});

    // populate job popup menus in client config tabs
    var template = $('#jobSelectPopup');
    $('td.jobSelectPopup').each(function ()
    {
        var jobId = $(this).text();
        $(this).empty().append(template.clone().show().val(jobId));
    });

    function DEBUG(strOrObj, objOrNada)
    {
        if (window.console && location.href.indexOf('debug=1') > -1)
        {
            if      (objOrNada)                    { console.log('tschenggins-status: ' + strOrObj + ': %o', objOrNada); }
            else if (typeof strOrObj === 'object') { console.log('tschenggins-status: %o', strOrObj); }
            else                                   { console.log('tschenggins-status: ' + strOrObj); }
        }
    }

});

/* ************************************************************************************************************** */
/*!
 * jQuery.scrollTo
 * Copyright (c) 2007 Ariel Flesler - aflesler ○ gmail • com | https://github.com/flesler
 * Licensed under MIT
 * https://github.com/flesler/jquery.scrollTo
 * @projectDescription Lightweight, cross-browser and highly customizable animated scrolling with jQuery
 * @author Ariel Flesler
 * @version 2.1.2
 */
;(function(factory) {
	'use strict';
	if (typeof define === 'function' && define.amd) {
		// AMD
		define(['jquery'], factory);
	} else if (typeof module !== 'undefined' && module.exports) {
		// CommonJS
		module.exports = factory(require('jquery'));
	} else {
		// Global
		factory(jQuery);
	}
})(function($) {
	'use strict';

	var $scrollTo = $.scrollTo = function(target, duration, settings) {
		return $(window).scrollTo(target, duration, settings);
	};

	$scrollTo.defaults = {
		axis:'xy',
		duration: 0,
		limit:true
	};

	function isWin(elem) {
		return !elem.nodeName ||
			$.inArray(elem.nodeName.toLowerCase(), ['iframe','#document','html','body']) !== -1;
	}

	$.fn.scrollTo = function(target, duration, settings) {
		if (typeof duration === 'object') {
			settings = duration;
			duration = 0;
		}
		if (typeof settings === 'function') {
			settings = { onAfter:settings };
		}
		if (target === 'max') {
			target = 9e9;
		}

		settings = $.extend({}, $scrollTo.defaults, settings);
		// Speed is still recognized for backwards compatibility
		duration = duration || settings.duration;
		// Make sure the settings are given right
		var queue = settings.queue && settings.axis.length > 1;
		if (queue) {
			// Let's keep the overall duration
			duration /= 2;
		}
		settings.offset = both(settings.offset);
		settings.over = both(settings.over);

		return this.each(function() {
			// Null target yields nothing, just like jQuery does
			if (target === null) return;

			var win = isWin(this),
				elem = win ? this.contentWindow || window : this,
				$elem = $(elem),
				targ = target,
				attr = {},
				toff;

			switch (typeof targ) {
				// A number will pass the regex
				case 'number':
				case 'string':
					if (/^([+-]=?)?\d+(\.\d+)?(px|%)?$/.test(targ)) {
						targ = both(targ);
						// We are done
						break;
					}
					// Relative/Absolute selector
					targ = win ? $(targ) : $(targ, elem);
					/* falls through */
				case 'object':
					if (targ.length === 0) return;
					// DOMElement / jQuery
					if (targ.is || targ.style) {
						// Get the real position of the target
						toff = (targ = $(targ)).offset();
					}
			}

			var offset = $.isFunction(settings.offset) && settings.offset(elem, targ) || settings.offset;

			$.each(settings.axis.split(''), function(i, axis) {
				var Pos	= axis === 'x' ? 'Left' : 'Top',
					pos = Pos.toLowerCase(),
					key = 'scroll' + Pos,
					prev = $elem[key](),
					max = $scrollTo.max(elem, axis);

				if (toff) {// jQuery / DOMElement
					attr[key] = toff[pos] + (win ? 0 : prev - $elem.offset()[pos]);

					// If it's a dom element, reduce the margin
					if (settings.margin) {
						attr[key] -= parseInt(targ.css('margin'+Pos), 10) || 0;
						attr[key] -= parseInt(targ.css('border'+Pos+'Width'), 10) || 0;
					}

					attr[key] += offset[pos] || 0;

					if (settings.over[pos]) {
						// Scroll to a fraction of its width/height
						attr[key] += targ[axis === 'x'?'width':'height']() * settings.over[pos];
					}
				} else {
					var val = targ[pos];
					// Handle percentage values
					attr[key] = val.slice && val.slice(-1) === '%' ?
						parseFloat(val) / 100 * max
						: val;
				}

				// Number or 'number'
				if (settings.limit && /^\d+$/.test(attr[key])) {
					// Check the limits
					attr[key] = attr[key] <= 0 ? 0 : Math.min(attr[key], max);
				}

				// Don't waste time animating, if there's no need.
				if (!i && settings.axis.length > 1) {
					if (prev === attr[key]) {
						// No animation needed
						attr = {};
					} else if (queue) {
						// Intermediate animation
						animate(settings.onAfterFirst);
						// Don't animate this axis again in the next iteration.
						attr = {};
					}
				}
			});

			animate(settings.onAfter);

			function animate(callback) {
				var opts = $.extend({}, settings, {
					// The queue setting conflicts with animate()
					// Force it to always be true
					queue: true,
					duration: duration,
					complete: callback && function() {
						callback.call(elem, targ, settings);
					}
				});
				$elem.animate(attr, opts);
			}
		});
	};

	// Max scrolling position, works on quirks mode
	// It only fails (not too badly) on IE, quirks mode.
	$scrollTo.max = function(elem, axis) {
		var Dim = axis === 'x' ? 'Width' : 'Height',
			scroll = 'scroll'+Dim;

		if (!isWin(elem))
			return elem[scroll] - $(elem)[Dim.toLowerCase()]();

		var size = 'client' + Dim,
			doc = elem.ownerDocument || elem.document,
			html = doc.documentElement,
			body = doc.body;

		return Math.max(html[scroll], body[scroll]) - Math.min(html[size], body[size]);
	};

	function both(val) {
		return $.isFunction(val) || $.isPlainObject(val) ? val : { top:val, left:val };
	}

	// Add special hooks so that window scroll properties can be animated
	$.Tween.propHooks.scrollLeft =
	$.Tween.propHooks.scrollTop = {
		get: function(t) {
			return $(t.elem)[t.prop]();
		},
		set: function(t) {
			var curr = this.get(t);
			// If interrupt is true and user scrolled, stop animating
			if (t.options.interrupt && t._last && t._last !== curr) {
				return $(t.elem).stop();
			}
			var next = Math.round(t.now);
			// Don't waste CPU
			// Browsers don't render floating point scroll
			if (curr !== next) {
				$(t.elem)[t.prop](next);
				t._last = this.get(t);
			}
		}
	};

	// AMD requirement
	return $scrollTo;
});
