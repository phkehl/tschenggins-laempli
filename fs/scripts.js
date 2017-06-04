/*
    Tschenggins Lämpli scripts
    Philipe Kehl <phkehl at oinkzwurgl dot org>
    https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

$( document ).ready(function()
{
    DEBUG('Tschenggins Lämpli! :-)');

    $('.menu a').each(function ()
    {
        if (this.href === location.href)
        {
            $(this).addClass('bold');
        }
    });

    $('div.cfg input[type=checkbox]').each(function ()
    {
        var cb = $(this);
        var name = cb.attr('name');
        var label = $('div.cfg label[for=' + name + ']');
        label.on('click', function (e)
        {
            cb.trigger('click');
        });
    });

    var divCfg = $('div.cfg');
    if (divCfg.length)
    {
        var divLeds = $('div.cfg div.leds');
        var ledIds = divLeds.data('ledids').split(' ');/*.map(function (s) { return parseInt(s, 16); });*/
        var ledTmpl = '<label for="ID">LED NN</label><select name="ID" id="ID" autocomplete="off">SS</select><br/>';
        divLeds.html('<label>Loading&hellip;</label>');
        var maxProgress = 67;
        var progressInt = setInterval(function ()
        {
            if (maxProgress-- > 0)
            {
                divLeds.append('.');
            }
            else
            {
                clearInterval(progressInt);
            }
        }, 300);

        $.ajax(
        {
            url: '/list', timeout: 20000, type: 'GET',
            complete: function(jqXHR, textStatus)
            {
                clearInterval(progressInt);
            },
            success: function(data, textStatus, jqXHR )
            {
                divLeds.empty().hide();
                var options = '<option value="00000000"></option>';
                //DEBUG(data);
                data.list.forEach(function (a)
                {
                    options += '<option value="' + a[0] + '">' + a[1] +'</option>';
                });
                ledIds.forEach(function (jobId, ix)
                {
                    var html = ledTmpl
                        .replace(/NN/g, (ix + 1))
                        .replace(/ID/g, 'led' + ('0' + ix).substr(-2))
                        .replace(/SS/, options);
                    divLeds.append(html);
                });
                ledIds.forEach(function (jobId, ix)
                {
                    var select = divLeds.find('select[id=led' + ('0' + ix).substr(-2) + ']');
                    select.val(jobId);
                });
                divLeds.show();
            },
            error: function(jqXHR, textStatus, errorThrown )
            {
                divLeds.html('<div class="cfgmsg error">Error: ' + textStatus +
                             ' (' + errorThrown + ').</br>LEDs configuration not available.' + '</div>');
            }
        });

        $('input[id=statusurl]').on('keyup', function (e)
        {
            var wdiv = $('#statusurl-warning');
            var val = $(this).val();
            if (val.length && !val.match(/(\.pl|\/)$/))
            {
                wdiv.html('The status URL should probably end in <em>.pl</em> or <em>/</em>.').show();
            }
            else
            {
                wdiv.empty().hide();
            }
        }).trigger('keyup');

    }

    function DEBUG(strOrObj, obj)
    {
        if (window.console && location.href.match(/debug=1/))
        {
            if (obj)
            {
                console.log('tl: ' + strOrObj + ': %o', obj);
            }
            else if (typeof strOrObj === 'object')
            {
                console.log('tl: %o', strOrObj);
            }
            else
            {
                console.log('tl: ' + strOrObj);
            }
        }
    }
});

/* eof */
